#ifndef FROSTD_FAN_H
#define FROSTD_FAN_H

#include <stdint.h>

/* Optional float value — present if valid != 0 */
typedef struct {
    double value;
    int    valid;
} opt_double_t;

typedef struct {
    char       name[64];
    opt_double_t rpm;
    opt_double_t percent;
} fan_reading_t;

typedef struct ipmi_ops ipmi_ops_t; /* forward declared in ipmi.h */

typedef struct {
    ipmi_ops_t *ops;
} fan_controller_t;

/*
 * Compute fan speed [0,100] using the parabolic easing curve.
 * Returns 0 if actual_temp <= ideal_temp, 100 if at or above max_temp.
 */
int suggest_speed(double actual_temp, double ideal_temp, double max_temp);

/*
 * Read current fan speeds from IPMI sensors.
 * out must point to an array of max fan_reading_t.
 * Returns number of readings on success, -1 on error.
 */
int fan_read_speeds(fan_controller_t *c,
                    fan_reading_t *out, int max,
                    char *errbuf, int errbuflen);

/*
 * Set chassis fan speed [0,100] via Dell OEM IPMI raw commands.
 * Returns 0 on success, -1 on error.
 */
int fan_set_speed(fan_controller_t *c, int percent,
                  char *errbuf, int errbuflen);

#endif /* FROSTD_FAN_H */
