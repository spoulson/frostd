#include "test_helpers.h"

#include <string.h>
#include <stdio.h>

/* ---- Mock IPMI vtable functions ---- */

static int mock_connect(void *impl) {
    mock_ipmi_ctx_t *m = impl;
    return m->connect_fail ? -1 : 0;
}

static void mock_close(void *impl) {
    (void)impl;
}

static int mock_get_temp_sensors(void *impl, temp_sensor_raw_t *out, int max) {
    mock_ipmi_ctx_t *m = impl;
    if (m->temp_fail) return -1;
    int n = (m->temp_count < max) ? m->temp_count : max;
    memcpy(out, m->temp_sensors, n * sizeof(*out));
    return n;
}

static int mock_get_fan_sensors(void *impl, fan_reading_raw_t *out, int max) {
    mock_ipmi_ctx_t *m = impl;
    if (m->fan_fail) return -1;
    int n = (m->fan_count < max) ? m->fan_count : max;
    memcpy(out, m->fan_sensors, n * sizeof(*out));
    return n;
}

static int mock_raw_command(void *impl, uint8_t netfn, uint8_t cmd,
                             const uint8_t *data, int data_len,
                             uint8_t *resp, int *resp_len) {
    mock_ipmi_ctx_t *m = impl;
    m->last_raw_netfn = netfn;
    m->last_raw_cmd   = cmd;
    int dlen = (data_len < 32) ? data_len : 32;
    memcpy(m->last_raw_data, data, dlen);
    m->last_raw_data_len = dlen;
    m->raw_call_count++;
    (void)resp; (void)resp_len;
    return m->raw_fail ? -1 : 0;
}

static const char *mock_last_error(void *impl) {
    mock_ipmi_ctx_t *m = impl;
    return m->errmsg[0] ? m->errmsg : "mock error";
}

ipmi_ops_t make_mock_ipmi(mock_ipmi_ctx_t *ctx) {
    return (ipmi_ops_t){
        .connect          = mock_connect,
        .close            = mock_close,
        .get_temp_sensors = mock_get_temp_sensors,
        .get_fan_sensors  = mock_get_fan_sensors,
        .raw_command      = mock_raw_command,
        .last_error       = mock_last_error,
        .impl             = ctx,
    };
}

/* ---- Mock command runner ---- */

static int mock_run(void *ctx,
                    const char *prog, const char *const *args,
                    char *buf, int buflen, char *errbuf, int errbuflen) {
    (void)prog; (void)args;
    mock_runner_ctx_t *r = ctx;
    if (!r->output) {
        snprintf(errbuf, errbuflen, "nvidia-smi not found");
        return -1;
    }
    int n = (int)strlen(r->output);
    if (n >= buflen) n = buflen - 1;
    memcpy(buf, r->output, n);
    buf[n] = '\0';
    return n;
}

cmd_runner_t make_mock_runner(mock_runner_ctx_t *ctx) {
    return (cmd_runner_t){mock_run, ctx};
}
