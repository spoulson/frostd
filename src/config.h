#ifndef FROSTD_CONFIG_H
#define FROSTD_CONFIG_H

#include <stdbool.h>

typedef struct {
    char *listen_addr;
} prometheus_config_t;

typedef struct {
    double  ideal_temp;
    double  max_temp;
    int     sample_size;
    char   *sample_interval; /* duration string e.g. "15s", parsed to seconds */
    double  sample_interval_sec; /* set by config_load, not from YAML */
} sensor_config_t;

typedef struct {
    char               *log_file;
    char               *log_format;
    bool                dry_run;
    char               *fan_log_interval; /* duration string */
    double              fan_log_interval_sec; /* set by config_load */
    prometheus_config_t *prometheus;
    sensor_config_t    *cpu;
    sensor_config_t    *gpu;
} frostd_config_t;

/*
 * Load, parse, apply defaults, and validate the config at path.
 * On success returns 0 and sets *cfg_out (caller must call config_free).
 * On error returns -1 and sets errbuf to a descriptive message.
 */
int  config_load(const char *path, frostd_config_t **cfg_out,
                 char *errbuf, int errbuflen);

void config_free(frostd_config_t *cfg);

/* Exposed for testing */
int parse_duration(const char *s, double *seconds_out);
int config_validate(const frostd_config_t *cfg, char *errbuf, int errbuflen);

#endif /* FROSTD_CONFIG_H */
