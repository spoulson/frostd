#ifndef FROSTD_IPMI_H
#define FROSTD_IPMI_H

#include <stdint.h>

#define IPMI_MAX_SENSORS 64

/* Raw sensor reading from IPMI for a temperature sensor */
typedef struct {
    char   name[64];
    double value;        /* degrees C */
    int    entity_id;
    int    entity_instance;
} temp_sensor_raw_t;

/* Raw fan sensor reading from IPMI */
typedef struct {
    char   name[64];
    int    has_rpm;
    double rpm;
    int    has_percent;
    double percent;
} fan_reading_raw_t;

/*
 * vtable for IPMI operations — production implementation uses FreeIPMI,
 * test implementations use mocks.
 */
struct ipmi_ops {
    /* Returns 0 on success, -1 on error */
    int  (*connect)(void *impl);
    void (*close)(void *impl);

    /* Returns number of readings on success, -1 on error */
    int  (*get_temp_sensors)(void *impl, temp_sensor_raw_t *out, int max);
    int  (*get_fan_sensors)(void *impl, fan_reading_raw_t *out, int max);

    /* Returns 0 on success, -1 on error */
    int  (*raw_command)(void *impl, uint8_t netfn, uint8_t cmd,
                        const uint8_t *data, int data_len,
                        uint8_t *resp, int *resp_len);

    /* Returns a human-readable string for the last error */
    const char *(*last_error)(void *impl);

    void *impl;
};

typedef struct ipmi_ops ipmi_ops_t;

/* Create a real (FreeIPMI-backed) IPMI ops instance.
 * Returns NULL on failure (logs reason to stderr). */
ipmi_ops_t *ipmi_ops_new_real(void);

/* Free an ipmi_ops_t created by ipmi_ops_new_real. */
void ipmi_ops_free(ipmi_ops_t *ops);

#endif /* FROSTD_IPMI_H */
