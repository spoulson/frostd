#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_helpers.h"
#include "../src/gpu.h"

/* ---- parse_gpu_temps tests ---- */

static void test_parse_single_gpu(void **state) {
    (void)state;
    char ids[16][64];
    double temps[16];
    char errbuf[256];
    int n = parse_gpu_temps("72\n", ids, temps, 16, errbuf, sizeof(errbuf));
    assert_int_equal(1, n);
    assert_string_equal("Temp", ids[0]);
    assert_float_equal(72.0, temps[0], 0.001);
}

static void test_parse_multiple_gpus(void **state) {
    (void)state;
    char ids[16][64];
    double temps[16];
    char errbuf[256];
    int n = parse_gpu_temps("68\n74\n", ids, temps, 16, errbuf, sizeof(errbuf));
    assert_int_equal(2, n);
    assert_string_equal("Temp",   ids[0]);
    assert_float_equal(68.0, temps[0], 0.001);
    assert_string_equal("Temp_2", ids[1]);
    assert_float_equal(74.0, temps[1], 0.001);
}

static void test_parse_empty_output(void **state) {
    (void)state;
    char ids[16][64];
    double temps[16];
    char errbuf[256];
    assert_int_equal(-1, parse_gpu_temps("", ids, temps, 16, errbuf, sizeof(errbuf)));
}

static void test_parse_invalid_line(void **state) {
    (void)state;
    char ids[16][64];
    double temps[16];
    char errbuf[256];
    assert_int_equal(-1, parse_gpu_temps("not a number\n", ids, temps, 16, errbuf, sizeof(errbuf)));
}

/* ---- gpu_read_temperatures tests ---- */

static void test_read_temperatures_calls_nvidia_smi(void **state) {
    (void)state;
    mock_runner_ctx_t rctx = {.output = "72\n"};
    cmd_runner_t runner = make_mock_runner(&rctx);
    gpu_reader_t *r = gpu_reader_new(runner);

    char ids[16][64];
    double temps[16];
    char errbuf[256];
    int n = gpu_read_temperatures(r, ids, temps, 16, errbuf, sizeof(errbuf));
    assert_int_equal(1, n);
    assert_string_equal("Temp", ids[0]);
    assert_float_equal(72.0, temps[0], 0.001);

    gpu_reader_free(r);
}

static void test_read_temperatures_command_error(void **state) {
    (void)state;
    mock_runner_ctx_t rctx = {.output = NULL};
    cmd_runner_t runner = make_mock_runner(&rctx);
    gpu_reader_t *r = gpu_reader_new(runner);

    char ids[16][64];
    double temps[16];
    char errbuf[256];
    assert_int_equal(-1, gpu_read_temperatures(r, ids, temps, 16, errbuf, sizeof(errbuf)));

    gpu_reader_free(r);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_parse_single_gpu),
        cmocka_unit_test(test_parse_multiple_gpus),
        cmocka_unit_test(test_parse_empty_output),
        cmocka_unit_test(test_parse_invalid_line),
        cmocka_unit_test(test_read_temperatures_calls_nvidia_smi),
        cmocka_unit_test(test_read_temperatures_command_error),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
