#include "fan.h"
#include "ipmi.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int suggest_speed(double actual_temp, double ideal_temp, double max_temp) {
    if (actual_temp <= ideal_temp) return 0;
    double delta1 = actual_temp - ideal_temp;
    double delta2 = max_temp   - ideal_temp;
    double speed  = (delta1 * delta1) * (100.0 / (delta2 * delta2));
    if (speed > 100.0) return 100;
    return (int)round(speed);
}

int fan_read_speeds(fan_controller_t *c,
                    fan_reading_t *out, int max,
                    char *errbuf, int errbuflen) {
    if (!c || !c->ops) {
        snprintf(errbuf, errbuflen, "fan controller not initialised");
        return -1;
    }

    if (c->ops->connect(c->ops->impl) != 0) {
        snprintf(errbuf, errbuflen, "connecting to IPMI: %s",
                 c->ops->last_error(c->ops->impl));
        return -1;
    }

    fan_reading_raw_t raw[IPMI_MAX_SENSORS];
    int n = c->ops->get_fan_sensors(c->ops->impl, raw, IPMI_MAX_SENSORS);
    c->ops->close(c->ops->impl);

    if (n < 0) {
        snprintf(errbuf, errbuflen, "getting IPMI fan sensors: %s",
                 c->ops->last_error(c->ops->impl));
        return -1;
    }

    int count = (n < max) ? n : max;
    for (int i = 0; i < count; i++) {
        strncpy(out[i].name, raw[i].name, sizeof(out[i].name) - 1);
        out[i].name[sizeof(out[i].name) - 1] = '\0';
        out[i].rpm.valid     = raw[i].has_rpm;
        out[i].rpm.value     = raw[i].rpm;
        out[i].percent.valid = raw[i].has_percent;
        out[i].percent.value = raw[i].percent;
    }
    return count;
}

int fan_set_speed(fan_controller_t *c, int percent,
                  char *errbuf, int errbuflen) {
    if (percent < 0 || percent > 100) {
        snprintf(errbuf, errbuflen, "fan speed %d out of range [0,100]", percent);
        return -1;
    }
    if (!c || !c->ops) {
        snprintf(errbuf, errbuflen, "fan controller not initialised");
        return -1;
    }

    if (c->ops->connect(c->ops->impl) != 0) {
        snprintf(errbuf, errbuflen, "connecting to IPMI: %s",
                 c->ops->last_error(c->ops->impl));
        return -1;
    }

    /* Dell OEM: enable manual fan control */
    uint8_t enable_data[] = {0x01, 0x00};
    uint8_t resp[16];
    int resp_len = (int)sizeof(resp);
    if (c->ops->raw_command(c->ops->impl, 0x30, 0x30,
                            enable_data, (int)sizeof(enable_data),
                            resp, &resp_len) != 0) {
        c->ops->close(c->ops->impl);
        snprintf(errbuf, errbuflen, "enabling manual fan control: %s",
                 c->ops->last_error(c->ops->impl));
        return -1;
    }

    /* Dell OEM: set fan speed */
    uint8_t speed_data[] = {0x02, 0xff, (uint8_t)percent};
    resp_len = (int)sizeof(resp);
    if (c->ops->raw_command(c->ops->impl, 0x30, 0x30,
                            speed_data, (int)sizeof(speed_data),
                            resp, &resp_len) != 0) {
        c->ops->close(c->ops->impl);
        snprintf(errbuf, errbuflen, "setting fan speed to %d%%: %s",
                 percent, c->ops->last_error(c->ops->impl));
        return -1;
    }

    c->ops->close(c->ops->impl);
    return 0;
}
