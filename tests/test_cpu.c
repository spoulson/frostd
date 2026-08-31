#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_helpers.h"
#include "../src/cpu.h"

static void test_returns_processor_temps(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {
        .temp_sensors = {
            {.name = "CPU1 Temp", .value = 45.0},
            {.name = "CPU2 Temp", .value = 50.0},
            {.name = "",          .value = 23.0},  /* inlet — no name → filtered */
        },
        .temp_count = 3,
    };
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    cpu_reader_t *r = cpu_reader_new(&ops);

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    int n = cpu_read_temperatures(r, ids, temps, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));

    assert_int_equal(2, n);
    double t_cpu1 = -1, t_cpu2 = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(ids[i], "CPU1 Temp") == 0) t_cpu1 = temps[i];
        if (strcmp(ids[i], "CPU2 Temp") == 0) t_cpu2 = temps[i];
    }
    assert_float_equal(45.0, t_cpu1, 0.001);
    assert_float_equal(50.0, t_cpu2, 0.001);

    cpu_reader_free(r);
}

static void test_deduplicates_sensor_names(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {
        .temp_sensors = {
            {.name = "Temp", .value = 45.0},
            {.name = "Temp", .value = 50.0},
            {.name = "Temp", .value = 55.0},
        },
        .temp_count = 3,
    };
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    cpu_reader_t *r = cpu_reader_new(&ops);

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    int n = cpu_read_temperatures(r, ids, temps, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));

    assert_int_equal(3, n);
    double t1 = -1, t2 = -1, t3 = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(ids[i], "Temp")   == 0) t1 = temps[i];
        if (strcmp(ids[i], "Temp_2") == 0) t2 = temps[i];
        if (strcmp(ids[i], "Temp_3") == 0) t3 = temps[i];
    }
    assert_float_equal(45.0, t1, 0.001);
    assert_float_equal(50.0, t2, 0.001);
    assert_float_equal(55.0, t3, 0.001);

    cpu_reader_free(r);
}

static void test_ignores_non_processor_sensors(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {
        .temp_sensors = {{.name = "Inlet Temp", .value = 23.0}},
        .temp_count   = 1,
    };
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    cpu_reader_t *r = cpu_reader_new(&ops);

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    int n = cpu_read_temperatures(r, ids, temps, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));
    assert_int_equal(-1, n);
    assert_non_null(strstr(errbuf, "no CPU temperature sensors found"));

    cpu_reader_free(r);
}

static void test_empty_sensor_list(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {.temp_count = 0};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    cpu_reader_t *r = cpu_reader_new(&ops);

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    assert_int_equal(-1,
        cpu_read_temperatures(r, ids, temps, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf)));

    cpu_reader_free(r);
}

static void test_connect_error(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {.connect_fail = 1};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    cpu_reader_t *r = cpu_reader_new(&ops);

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    assert_int_equal(-1,
        cpu_read_temperatures(r, ids, temps, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf)));

    cpu_reader_free(r);
}

static void test_sensors_error(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {.temp_fail = 1};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    cpu_reader_t *r = cpu_reader_new(&ops);

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    assert_int_equal(-1,
        cpu_read_temperatures(r, ids, temps, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf)));

    cpu_reader_free(r);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_returns_processor_temps),
        cmocka_unit_test(test_deduplicates_sensor_names),
        cmocka_unit_test(test_ignores_non_processor_sensors),
        cmocka_unit_test(test_empty_sensor_list),
        cmocka_unit_test(test_connect_error),
        cmocka_unit_test(test_sensors_error),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
