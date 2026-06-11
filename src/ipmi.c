#include "ipmi.h"

#include <ipmi_monitoring.h>
#include <ipmi_monitoring_bitmasks.h>
#include <freeipmi/api/ipmi-api.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* CPU temperature sensors use entity ID 0x03 (processor).
 * libipmimonitoring does not expose entity ID directly, so we filter by
 * sensor type Temperature and rely on the sensor name prefix check being
 * unnecessary — all Temperature sensors at entity 0x03 are CPU packages.
 * In practice, the server also reports inlet/exhaust sensors but those
 * have different sensor types (temp is type 0x01 for all, entity differs).
 *
 * The filtering by entity ID is done in cpu.c rather than here, since
 * libipmimonitoring does not provide direct entity ID access in the
 * iteration API of this version. cpu.c uses the record_id list approach
 * via sensor_readings_by_record_id if needed; for now we expose all
 * temperature sensors and let cpu.c filter by name convention.
 */

#define SDR_CACHE_DIR "/var/lib/frostd"

/* ---- implementation context ---- */

typedef struct {
    ipmi_monitoring_ctx_t mon_ctx;  /* for sensor reads */
    ipmi_ctx_t            raw_ctx;  /* for raw commands */
    char                  errmsg[256];
} real_ipmi_impl_t;

static int real_connect(void *impl) {
    real_ipmi_impl_t *r = impl;

    /* Initialise ipmimonitoring (idempotent) */
    int errnum;
    if (ipmi_monitoring_init(0, &errnum) < 0) {
        snprintf(r->errmsg, sizeof(r->errmsg),
                 "ipmi_monitoring_init: errnum %d", errnum);
        return -1;
    }

    /* Create monitoring context */
    if (!r->mon_ctx) {
        r->mon_ctx = ipmi_monitoring_ctx_create();
        if (!r->mon_ctx) {
            snprintf(r->errmsg, sizeof(r->errmsg), "ipmi_monitoring_ctx_create failed");
            return -1;
        }
        ipmi_monitoring_ctx_sdr_cache_directory(r->mon_ctx, SDR_CACHE_DIR);
    }

    /* Create raw IPMI context */
    if (!r->raw_ctx) {
        r->raw_ctx = ipmi_ctx_create();
        if (!r->raw_ctx) {
            snprintf(r->errmsg, sizeof(r->errmsg), "ipmi_ctx_create failed");
            return -1;
        }
        /* Auto-discover inband IPMI device */
        int rc = ipmi_ctx_find_inband(r->raw_ctx, NULL, 0, 0, 0, NULL, 0, 0);
        if (rc < 0) {
            snprintf(r->errmsg, sizeof(r->errmsg),
                     "ipmi_ctx_find_inband: %s", ipmi_ctx_errormsg(r->raw_ctx));
            return -1;
        }
        if (rc == 0) {
            snprintf(r->errmsg, sizeof(r->errmsg), "no inband IPMI device found");
            return -1;
        }
    }
    return 0;
}

static void real_close(void *impl) {
    real_ipmi_impl_t *r = impl;
    if (r->mon_ctx) {
        ipmi_monitoring_ctx_destroy(r->mon_ctx);
        r->mon_ctx = NULL;
    }
    if (r->raw_ctx) {
        ipmi_ctx_close(r->raw_ctx);
        ipmi_ctx_destroy(r->raw_ctx);
        r->raw_ctx = NULL;
    }
}

static int read_sensors_by_type(real_ipmi_impl_t *r, unsigned int sensor_type,
                                 temp_sensor_raw_t *temp_out, int temp_max,
                                 fan_reading_raw_t *fan_out, int fan_max) {
    unsigned int types[] = {sensor_type};
    int rc = ipmi_monitoring_sensor_readings_by_sensor_type(
        r->mon_ctx, NULL, NULL,
        IPMI_MONITORING_SENSOR_READING_FLAGS_IGNORE_NON_INTERPRETABLE_SENSORS,
        types, 1, NULL, NULL);
    if (rc < 0) {
        snprintf(r->errmsg, sizeof(r->errmsg),
                 "sensor_readings_by_sensor_type: %s",
                 ipmi_monitoring_ctx_errormsg(r->mon_ctx));
        return -1;
    }

    int count = 0;
    if (ipmi_monitoring_sensor_iterator_first(r->mon_ctx) < 0)
        return 0;

    do {
        const char *name = ipmi_monitoring_sensor_read_sensor_name(r->mon_ctx);
        int   units = ipmi_monitoring_sensor_read_sensor_units(r->mon_ctx);
        int   rtype = ipmi_monitoring_sensor_read_sensor_reading_type(r->mon_ctx);
        void *val_p = ipmi_monitoring_sensor_read_sensor_reading(r->mon_ctx);

        if (!name || !val_p) continue;
        if (rtype != IPMI_MONITORING_SENSOR_READING_TYPE_DOUBLE) continue;
        double val = *(double *)val_p;

        if (sensor_type == IPMI_MONITORING_SENSOR_TYPE_TEMPERATURE &&
            temp_out && count < temp_max) {
            strncpy(temp_out[count].name, name, sizeof(temp_out[count].name) - 1);
            temp_out[count].name[sizeof(temp_out[count].name) - 1] = '\0';
            temp_out[count].value = val;
            count++;
        } else if (sensor_type == IPMI_MONITORING_SENSOR_TYPE_FAN &&
                   fan_out && count < fan_max) {
            strncpy(fan_out[count].name, name, sizeof(fan_out[count].name) - 1);
            fan_out[count].name[sizeof(fan_out[count].name) - 1] = '\0';
            if (units == IPMI_MONITORING_SENSOR_UNITS_RPM) {
                fan_out[count].has_rpm = 1;
                fan_out[count].rpm     = val;
            } else if (units == IPMI_MONITORING_SENSOR_UNITS_PERCENT) {
                fan_out[count].has_percent = 1;
                fan_out[count].percent     = val;
            }
            count++;
        }
    } while (ipmi_monitoring_sensor_iterator_next(r->mon_ctx) == 0);

    return count;
}

static int real_get_temp_sensors(void *impl, temp_sensor_raw_t *out, int max) {
    real_ipmi_impl_t *r = impl;
    return read_sensors_by_type(r, IPMI_MONITORING_SENSOR_TYPE_TEMPERATURE,
                                 out, max, NULL, 0);
}

static int real_get_fan_sensors(void *impl, fan_reading_raw_t *out, int max) {
    real_ipmi_impl_t *r = impl;
    return read_sensors_by_type(r, IPMI_MONITORING_SENSOR_TYPE_FAN,
                                 NULL, 0, out, max);
}

static int real_raw_command(void *impl, uint8_t netfn, uint8_t cmd,
                             const uint8_t *data, int data_len,
                             uint8_t *resp, int *resp_len) {
    real_ipmi_impl_t *r = impl; /* needs write access for errmsg */
    uint8_t rs_buf[256];
    int rs_len = (int)sizeof(rs_buf);
    int rc = ipmi_cmd_raw(r->raw_ctx, 0, netfn, data, data_len, rs_buf, rs_len);
    (void)cmd;
    if (rc < 0) {
        snprintf(r->errmsg, sizeof(r->errmsg),
                 "ipmi_cmd_raw: %s", ipmi_ctx_errormsg(r->raw_ctx));
        return -1;
    }
    if (resp && resp_len && *resp_len > 0) {
        int copy = (rc < *resp_len) ? rc : *resp_len;
        memcpy(resp, rs_buf, copy);
        *resp_len = copy;
    }
    return 0;
}

static const char *real_last_error(void *impl) {
    const real_ipmi_impl_t *r = impl;
    return r->errmsg;
}

/* ---- public API ---- */

ipmi_ops_t *ipmi_ops_new_real(void) {
    real_ipmi_impl_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;

    ipmi_ops_t *ops = malloc(sizeof(*ops));
    if (!ops) { free(r); return NULL; }

    *ops = (ipmi_ops_t){
        .connect          = real_connect,
        .close            = real_close,
        .get_temp_sensors = real_get_temp_sensors,
        .get_fan_sensors  = real_get_fan_sensors,
        .raw_command      = real_raw_command,
        .last_error       = real_last_error,
        .impl             = r,
    };
    return ops;
}

void ipmi_ops_free(ipmi_ops_t *ops) {
    if (!ops) return;
    real_ipmi_impl_t *r = ops->impl;
    real_close(r);
    free(r);
    free(ops);
}
