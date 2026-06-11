#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/metrics.h"
#include "../src/config.h"

/* --- static test reader --- */
typedef struct {
    char   ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS];
    int    count;
    int    fail; /* non-zero to return error */
} static_reader_ctx_t;

static int static_read(void *ctx,
                       char out_ids[][64], double *out_temps, int max,
                       char *errbuf, int errbuflen) {
    static_reader_ctx_t *r = ctx;
    if (r->fail) {
        snprintf(errbuf, errbuflen, "ipmitool failed");
        return -1;
    }
    int n = (r->count < max) ? r->count : max;
    for (int i = 0; i < n; i++) {
        strncpy(out_ids[i], r->ids[i], 64);
        out_temps[i] = r->temps[i];
    }
    return n;
}

static sensor_config_t *make_sensor_config(int sample_size) {
    sensor_config_t *cfg = calloc(1, sizeof(*cfg));
    cfg->ideal_temp = 40.0;
    cfg->max_temp   = 75.0;
    cfg->sample_size = sample_size;
    cfg->sample_interval_sec = 15.0;
    return cfg;
}

/* --- tests --- */

static void test_accumulates_samples(void **state) {
    (void)state;
    static_reader_ctx_t rctx = {
        .ids    = {"s0", "s1"},
        .temps  = {50.0, 60.0},
        .count  = 2,
    };
    sensor_config_t *cfg = make_sensor_config(3);
    sensor_monitor_t *m = sensor_monitor_new("cpu", cfg, (temp_reader_t){static_read, &rctx});

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS], aggs[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    int n = sensor_monitor_poll(m, ids, temps, aggs, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));

    assert_int_equal(2, n);
    /* find s0 and s1 */
    double agg_s0 = -1, agg_s1 = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(ids[i], "s0") == 0) agg_s0 = aggs[i];
        if (strcmp(ids[i], "s1") == 0) agg_s1 = aggs[i];
    }
    assert_float_equal(50.0, agg_s0, 0.001);
    assert_float_equal(60.0, agg_s1, 0.001);

    sensor_monitor_free(m);
    free(cfg);
}

static void test_caps_samples_at_sample_size(void **state) {
    (void)state;
    static_reader_ctx_t rctx = {.ids = {"s0"}, .temps = {0.0}, .count = 1};
    sensor_config_t *cfg = make_sensor_config(3);
    sensor_monitor_t *m = sensor_monitor_new("cpu", cfg, (temp_reader_t){static_read, &rctx});

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS], aggs[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];

    double poll_vals[] = {0.0, 10.0, 20.0, 30.0, 40.0};
    for (int i = 0; i < 5; i++) {
        rctx.temps[0] = poll_vals[i];
        sensor_monitor_poll(m, ids, temps, aggs, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));
    }

    /* After 5 polls with sample_size=3, last 3 values are 20, 30, 40 */
    char agg_ids[METRICS_MAX_SENSOR_IDS][64];
    double agg_vals[METRICS_MAX_SENSOR_IDS];
    int nc = sensor_monitor_aggregates(m, agg_ids, agg_vals, METRICS_MAX_SENSOR_IDS);
    assert_int_equal(1, nc);
    assert_float_equal((20.0 + 30.0 + 40.0) / 3.0, agg_vals[0], 0.001);

    sensor_monitor_free(m);
    free(cfg);
}

static void test_aggregates_before_any_polls(void **state) {
    (void)state;
    static_reader_ctx_t rctx = {.ids = {"s0"}, .temps = {50.0}, .count = 1};
    sensor_config_t *cfg = make_sensor_config(3);
    sensor_monitor_t *m = sensor_monitor_new("cpu", cfg, (temp_reader_t){static_read, &rctx});

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double aggs[METRICS_MAX_SENSOR_IDS];
    int n = sensor_monitor_aggregates(m, ids, aggs, METRICS_MAX_SENSOR_IDS);
    assert_int_equal(0, n);

    sensor_monitor_free(m);
    free(cfg);
}

static void test_reader_error(void **state) {
    (void)state;
    static_reader_ctx_t rctx = {.fail = 1};
    sensor_config_t *cfg = make_sensor_config(3);
    sensor_monitor_t *m = sensor_monitor_new("cpu", cfg, (temp_reader_t){static_read, &rctx});

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS], aggs[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    int n = sensor_monitor_poll(m, ids, temps, aggs, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));
    assert_int_equal(-1, n);

    sensor_monitor_free(m);
    free(cfg);
}

static void test_empty_readings(void **state) {
    (void)state;
    static_reader_ctx_t rctx = {.count = 0};
    sensor_config_t *cfg = make_sensor_config(3);
    sensor_monitor_t *m = sensor_monitor_new("cpu", cfg, (temp_reader_t){static_read, &rctx});

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS], aggs[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];
    int n = sensor_monitor_poll(m, ids, temps, aggs, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));
    assert_int_equal(-1, n);

    sensor_monitor_free(m);
    free(cfg);
}

static void test_multiple_sensor_ids_independent_aggregates(void **state) {
    (void)state;
    static_reader_ctx_t rctx = {
        .ids   = {"s0", "s1"},
        .temps = {50.0, 70.0},
        .count = 2,
    };
    sensor_config_t *cfg = make_sensor_config(2);
    sensor_monitor_t *m = sensor_monitor_new("cpu", cfg, (temp_reader_t){static_read, &rctx});

    char ids[METRICS_MAX_SENSOR_IDS][64];
    double temps[METRICS_MAX_SENSOR_IDS], aggs[METRICS_MAX_SENSOR_IDS];
    char errbuf[256];

    sensor_monitor_poll(m, ids, temps, aggs, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));

    rctx.temps[0] = 60.0;
    rctx.temps[1] = 80.0;
    int n = sensor_monitor_poll(m, ids, temps, aggs, METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));
    assert_int_equal(2, n);

    double agg_s0 = -1, agg_s1 = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(ids[i], "s0") == 0) agg_s0 = aggs[i];
        if (strcmp(ids[i], "s1") == 0) agg_s1 = aggs[i];
    }
    assert_float_equal((50.0 + 60.0) / 2.0, agg_s0, 0.001);
    assert_float_equal((70.0 + 80.0) / 2.0, agg_s1, 0.001);

    sensor_monitor_free(m);
    free(cfg);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_accumulates_samples),
        cmocka_unit_test(test_caps_samples_at_sample_size),
        cmocka_unit_test(test_aggregates_before_any_polls),
        cmocka_unit_test(test_reader_error),
        cmocka_unit_test(test_empty_readings),
        cmocka_unit_test(test_multiple_sensor_ids_independent_aggregates),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
