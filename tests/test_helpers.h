#ifndef FROSTD_TEST_HELPERS_H
#define FROSTD_TEST_HELPERS_H

#include <stdint.h>

#include "../src/ipmi.h"
#include "../src/metrics.h"

/* ---- Mock IPMI context ---- */

typedef struct {
    temp_sensor_raw_t  temp_sensors[IPMI_MAX_SENSORS];
    int                temp_count;
    fan_reading_raw_t  fan_sensors[IPMI_MAX_SENSORS];
    int                fan_count;
    int                connect_fail;
    int                temp_fail;
    int                fan_fail;
    int                raw_fail;
    uint8_t            last_raw_netfn;
    uint8_t            last_raw_cmd;
    uint8_t            last_raw_data[32];
    int                last_raw_data_len;
    int                raw_call_count;
    char               errmsg[128];
} mock_ipmi_ctx_t;

ipmi_ops_t make_mock_ipmi(mock_ipmi_ctx_t *ctx);

/* ---- Mock command runner ---- */

typedef struct {
    const char *output; /* NULL → return error */
} mock_runner_ctx_t;

/* Returns a cmd_runner_t backed by mock_runner_ctx_t */
#include "../src/gpu.h"
cmd_runner_t make_mock_runner(mock_runner_ctx_t *ctx);

#endif /* FROSTD_TEST_HELPERS_H */
