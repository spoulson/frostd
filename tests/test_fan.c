#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"
#include "../src/fan.h"

/* ---- SuggestSpeed tests ---- */

static void test_suggest_speed_at_ideal(void **state) {
    (void)state;
    assert_int_equal(0, suggest_speed(40.0, 40.0, 75.0));
}

static void test_suggest_speed_below_ideal(void **state) {
    (void)state;
    assert_int_equal(0, suggest_speed(35.0, 40.0, 75.0));
}

static void test_suggest_speed_at_max(void **state) {
    (void)state;
    assert_int_equal(100, suggest_speed(75.0, 40.0, 75.0));
}

static void test_suggest_speed_above_max(void **state) {
    (void)state;
    assert_int_equal(100, suggest_speed(90.0, 40.0, 75.0));
}

static void test_suggest_speed_midpoint(void **state) {
    (void)state;
    /* ideal=40, max=75, actual=57.5 → (17.5)^2 * (100/1225) = 25 */
    assert_int_equal(25, suggest_speed(57.5, 40.0, 75.0));
}

static void test_suggest_speed_three_quarter(void **state) {
    (void)state;
    /* actual = 40 + 0.75*35 = 66.25 → (26.25)^2 * (100/1225) ≈ 56.25 → 56 */
    assert_int_equal(56, suggest_speed(66.25, 40.0, 75.0));
}

/* ---- fan_set_speed tests ---- */

static void test_set_speed_ok(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {0};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    fan_controller_t ctrl = {.ops = &ops};
    char errbuf[256];
    assert_int_equal(0, fan_set_speed(&ctrl, 50, errbuf, sizeof(errbuf)));
    assert_int_equal(2, mctx.raw_call_count);
}

static void test_set_speed_out_of_range(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {0};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    fan_controller_t ctrl = {.ops = &ops};
    char errbuf[256];
    assert_int_equal(-1, fan_set_speed(&ctrl, 101, errbuf, sizeof(errbuf)));
    assert_int_equal(-1, fan_set_speed(&ctrl, -1,  errbuf, sizeof(errbuf)));
}

static void test_set_speed_ipmi_error(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {.raw_fail = 1};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    fan_controller_t ctrl = {.ops = &ops};
    char errbuf[256];
    assert_int_equal(-1, fan_set_speed(&ctrl, 50, errbuf, sizeof(errbuf)));
}

static void test_set_speed_connect_error(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {.connect_fail = 1};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    fan_controller_t ctrl = {.ops = &ops};
    char errbuf[256];
    assert_int_equal(-1, fan_set_speed(&ctrl, 50, errbuf, sizeof(errbuf)));
}

/* ---- fan_read_speeds tests ---- */

static void test_read_fan_speeds_rpm(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {
        .fan_sensors = {{
            .name = "Fan1",
            .has_rpm = 1, .rpm = 3600.0,
            .has_percent = 0,
        }},
        .fan_count = 1,
    };
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    fan_controller_t ctrl = {.ops = &ops};
    fan_reading_t readings[16];
    char errbuf[256];
    int n = fan_read_speeds(&ctrl, readings, 16, errbuf, sizeof(errbuf));
    assert_int_equal(1, n);
    assert_string_equal("Fan1", readings[0].name);
    assert_true(readings[0].rpm.valid);
    assert_float_equal(3600.0, readings[0].rpm.value, 0.001);
    assert_false(readings[0].percent.valid);
}

static void test_read_fan_speeds_percent(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {
        .fan_sensors = {{
            .name = "Fan1",
            .has_percent = 1, .percent = 50.0,
            .has_rpm = 0,
        }},
        .fan_count = 1,
    };
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    fan_controller_t ctrl = {.ops = &ops};
    fan_reading_t readings[16];
    char errbuf[256];
    int n = fan_read_speeds(&ctrl, readings, 16, errbuf, sizeof(errbuf));
    assert_int_equal(1, n);
    assert_true(readings[0].percent.valid);
    assert_float_equal(50.0, readings[0].percent.value, 0.001);
    assert_false(readings[0].rpm.valid);
}

static void test_read_fan_speeds_ipmi_error(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {.fan_fail = 1};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    fan_controller_t ctrl = {.ops = &ops};
    fan_reading_t readings[16];
    char errbuf[256];
    assert_int_equal(-1, fan_read_speeds(&ctrl, readings, 16, errbuf, sizeof(errbuf)));
}

static void test_read_fan_speeds_connect_error(void **state) {
    (void)state;
    mock_ipmi_ctx_t mctx = {.connect_fail = 1};
    ipmi_ops_t ops = make_mock_ipmi(&mctx);
    fan_controller_t ctrl = {.ops = &ops};
    fan_reading_t readings[16];
    char errbuf[256];
    assert_int_equal(-1, fan_read_speeds(&ctrl, readings, 16, errbuf, sizeof(errbuf)));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_suggest_speed_at_ideal),
        cmocka_unit_test(test_suggest_speed_below_ideal),
        cmocka_unit_test(test_suggest_speed_at_max),
        cmocka_unit_test(test_suggest_speed_above_max),
        cmocka_unit_test(test_suggest_speed_midpoint),
        cmocka_unit_test(test_suggest_speed_three_quarter),
        cmocka_unit_test(test_set_speed_ok),
        cmocka_unit_test(test_set_speed_out_of_range),
        cmocka_unit_test(test_set_speed_ipmi_error),
        cmocka_unit_test(test_set_speed_connect_error),
        cmocka_unit_test(test_read_fan_speeds_rpm),
        cmocka_unit_test(test_read_fan_speeds_percent),
        cmocka_unit_test(test_read_fan_speeds_ipmi_error),
        cmocka_unit_test(test_read_fan_speeds_connect_error),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
