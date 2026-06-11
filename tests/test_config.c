#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/config.h"

static char g_tmpfile[256];

static char *write_temp_config(const char *content) {
    strcpy(g_tmpfile, "/tmp/frostd_test_XXXXXX.yaml");
    int fd = mkstemps(g_tmpfile, 5);
    if (fd < 0) return NULL;
    write(fd, content, strlen(content));
    close(fd);
    return g_tmpfile;
}

static int teardown(void **state) {
    (void)state;
    if (g_tmpfile[0]) unlink(g_tmpfile);
    g_tmpfile[0] = '\0';
    return 0;
}

static void test_happy_path(void **state) {
    (void)state;
    char *path = write_temp_config(
        "log_file: /var/log/frostd/frostd.log\n"
        "cpu:\n"
        "  ideal_temp: 45\n"
        "  max_temp: 80\n"
        "  sample_size: 5\n"
        "  sample_interval: 10s\n"
        "gpu:\n"
        "  ideal_temp: 50\n"
        "  max_temp: 85\n"
        "  sample_size: 4\n"
        "  sample_interval: 20s\n");
    assert_non_null(path);

    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
    assert_non_null(cfg);
    assert_string_equal("/var/log/frostd/frostd.log", cfg->log_file);
    assert_false(cfg->dry_run);
    assert_non_null(cfg->cpu);
    assert_float_equal(45.0, cfg->cpu->ideal_temp, 0.001);
    assert_float_equal(80.0, cfg->cpu->max_temp, 0.001);
    assert_int_equal(5, cfg->cpu->sample_size);
    assert_float_equal(10.0, cfg->cpu->sample_interval_sec, 0.001);
    assert_non_null(cfg->gpu);
    assert_float_equal(50.0, cfg->gpu->ideal_temp, 0.001);
    config_free(cfg);
}

static void test_dry_run_enabled(void **state) {
    (void)state;
    char *path = write_temp_config(
        "dry_run: true\n"
        "cpu:\n"
        "  ideal_temp: 40\n"
        "  max_temp: 75\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
    assert_true(cfg->dry_run);
    config_free(cfg);
}

static void test_dry_run_default(void **state) {
    (void)state;
    char *path = write_temp_config("cpu: {}\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
    assert_false(cfg->dry_run);
    config_free(cfg);
}

static void test_defaults(void **state) {
    (void)state;
    char *path = write_temp_config("cpu: {}\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
    assert_string_equal("text", cfg->log_format);
    assert_float_equal(40.0, cfg->cpu->ideal_temp, 0.001);
    assert_float_equal(75.0, cfg->cpu->max_temp, 0.001);
    assert_int_equal(3, cfg->cpu->sample_size);
    assert_float_equal(15.0, cfg->cpu->sample_interval_sec, 0.001);
    config_free(cfg);
}

static void test_log_format_json(void **state) {
    (void)state;
    char *path = write_temp_config("log_format: json\ncpu: {}\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
    assert_string_equal("json", cfg->log_format);
    config_free(cfg);
}

static void test_log_format_invalid(void **state) {
    (void)state;
    char *path = write_temp_config("log_format: xml\ncpu: {}\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    int rc = config_load(path, &cfg, errbuf, sizeof(errbuf));
    assert_int_not_equal(0, rc);
    assert_non_null(strstr(errbuf, "log_format"));
}

static void test_no_devices(void **state) {
    (void)state;
    char *path = write_temp_config("log_file: /tmp/test.log\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    int rc = config_load(path, &cfg, errbuf, sizeof(errbuf));
    assert_int_not_equal(0, rc);
    assert_non_null(strstr(errbuf, "at least one sensor type"));
}

static void test_ideal_temp_equal_max_temp(void **state) {
    (void)state;
    char *path = write_temp_config(
        "cpu:\n"
        "  ideal_temp: 75\n"
        "  max_temp: 75\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_not_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
}

static void test_ideal_temp_greater_than_max_temp(void **state) {
    (void)state;
    char *path = write_temp_config(
        "cpu:\n"
        "  ideal_temp: 80\n"
        "  max_temp: 75\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_not_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
}

static void test_sample_size_negative(void **state) {
    (void)state;
    char *path = write_temp_config(
        "cpu:\n"
        "  ideal_temp: 40\n"
        "  max_temp: 75\n"
        "  sample_size: -1\n"
        "  sample_interval: 15s\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    int rc = config_load(path, &cfg, errbuf, sizeof(errbuf));
    assert_int_not_equal(0, rc);
    assert_non_null(strstr(errbuf, "sample_size"));
}

static void test_negative_sample_interval(void **state) {
    (void)state;
    char *path = write_temp_config(
        "cpu:\n"
        "  ideal_temp: 40\n"
        "  max_temp: 75\n"
        "  sample_size: 3\n"
        "  sample_interval: -5s\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    int rc = config_load(path, &cfg, errbuf, sizeof(errbuf));
    assert_int_not_equal(0, rc);
    assert_non_null(strstr(errbuf, "sample_interval"));
}

static void test_invalid_yaml(void **state) {
    (void)state;
    char *path = write_temp_config(":::invalid yaml:::\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_not_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
}

static void test_missing_file(void **state) {
    (void)state;
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_not_equal(0,
        config_load("/nonexistent/path/frostd.yaml", &cfg, errbuf, sizeof(errbuf)));
}

static void test_prometheus_with_listen_addr(void **state) {
    (void)state;
    char *path = write_temp_config(
        "cpu: {}\n"
        "prometheus:\n"
        "  listen_addr: \":9100\"\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
    assert_non_null(cfg->prometheus);
    assert_string_equal(":9100", cfg->prometheus->listen_addr);
    config_free(cfg);
}

static void test_prometheus_absent(void **state) {
    (void)state;
    char *path = write_temp_config("cpu: {}\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
    assert_null(cfg->prometheus);
    config_free(cfg);
}

static void test_prometheus_empty_listen_addr(void **state) {
    (void)state;
    char *path = write_temp_config("cpu: {}\nprometheus: {}\n");
    frostd_config_t *cfg = NULL;
    char errbuf[256];
    assert_int_equal(0, config_load(path, &cfg, errbuf, sizeof(errbuf)));
    assert_non_null(cfg->prometheus);
    /* listen_addr omitted → NULL pointer */
    assert_null(cfg->prometheus->listen_addr);
    config_free(cfg);
}

static void test_parse_duration_seconds(void **state) {
    (void)state;
    double sec;
    assert_int_equal(0, parse_duration("15s", &sec));
    assert_float_equal(15.0, sec, 0.001);
}

static void test_parse_duration_minutes(void **state) {
    (void)state;
    double sec;
    assert_int_equal(0, parse_duration("2m", &sec));
    assert_float_equal(120.0, sec, 0.001);
}

static void test_parse_duration_hours(void **state) {
    (void)state;
    double sec;
    assert_int_equal(0, parse_duration("1h", &sec));
    assert_float_equal(3600.0, sec, 0.001);
}

static void test_parse_duration_bare_number(void **state) {
    (void)state;
    double sec;
    assert_int_equal(0, parse_duration("30", &sec));
    assert_float_equal(30.0, sec, 0.001);
}

static void test_parse_duration_negative(void **state) {
    (void)state;
    double sec;
    assert_int_not_equal(0, parse_duration("-5s", &sec));
}

static void test_parse_duration_invalid(void **state) {
    (void)state;
    double sec;
    assert_int_not_equal(0, parse_duration("abc", &sec));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_teardown(test_happy_path, teardown),
        cmocka_unit_test_teardown(test_dry_run_enabled, teardown),
        cmocka_unit_test_teardown(test_dry_run_default, teardown),
        cmocka_unit_test_teardown(test_defaults, teardown),
        cmocka_unit_test_teardown(test_log_format_json, teardown),
        cmocka_unit_test_teardown(test_log_format_invalid, teardown),
        cmocka_unit_test_teardown(test_no_devices, teardown),
        cmocka_unit_test_teardown(test_ideal_temp_equal_max_temp, teardown),
        cmocka_unit_test_teardown(test_ideal_temp_greater_than_max_temp, teardown),
        cmocka_unit_test_teardown(test_sample_size_negative, teardown),
        cmocka_unit_test_teardown(test_negative_sample_interval, teardown),
        cmocka_unit_test_teardown(test_invalid_yaml, teardown),
        cmocka_unit_test_teardown(test_missing_file, teardown),
        cmocka_unit_test_teardown(test_prometheus_with_listen_addr, teardown),
        cmocka_unit_test_teardown(test_prometheus_absent, teardown),
        cmocka_unit_test_teardown(test_prometheus_empty_listen_addr, teardown),
        cmocka_unit_test(test_parse_duration_seconds),
        cmocka_unit_test(test_parse_duration_minutes),
        cmocka_unit_test(test_parse_duration_hours),
        cmocka_unit_test(test_parse_duration_bare_number),
        cmocka_unit_test(test_parse_duration_negative),
        cmocka_unit_test(test_parse_duration_invalid),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
