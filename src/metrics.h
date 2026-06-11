#ifndef FROSTD_METRICS_H
#define FROSTD_METRICS_H

#include "config.h"
#include <stddef.h>

#define METRICS_MAX_SENSOR_IDS 32

/* Interface for reading temperatures — analogous to Go's TempReader interface. */
typedef struct {
    /* Returns number of readings written into ids/temps (max count).
     * ids must be an array of char[64]. Returns -1 on error. */
    int (*read_temperatures)(void *ctx,
                             char ids[][64], double *temps, int max,
                             char *errbuf, int errbuflen);
    void *ctx;
} temp_reader_t;

typedef struct {
    char   id[64];
    double *samples;     /* circular buffer, length = cfg->sample_size */
    int     head;        /* next write index */
    int     count;       /* number of valid samples */
} sensor_buf_t;

typedef struct {
    const char      *name;
    sensor_config_t *cfg;
    temp_reader_t    reader;
    sensor_buf_t     bufs[METRICS_MAX_SENSOR_IDS];
    int              buf_count;
} sensor_monitor_t;

/* Result of Poll: arrays of sensor IDs, latest temps, and aggregate temps.
 * out_ids, out_temps, out_aggs must be arrays of at least METRICS_MAX_SENSOR_IDS.
 * out_ids rows are char[64]. Returns number of sensors, or -1 on error. */
int sensor_monitor_poll(sensor_monitor_t *m,
                        char out_ids[][64], double *out_temps,
                        double *out_aggs, int max,
                        char *errbuf, int errbuflen);

/* Returns aggregate (rolling average) for each sensor currently buffered.
 * Same out arrays as above. Returns count. */
int sensor_monitor_aggregates(const sensor_monitor_t *m,
                               char out_ids[][64], double *out_aggs, int max);

/* Allocate and initialise a sensor monitor. */
sensor_monitor_t *sensor_monitor_new(const char *name, sensor_config_t *cfg,
                                     temp_reader_t reader);

/* Free a sensor monitor created with sensor_monitor_new. */
void sensor_monitor_free(sensor_monitor_t *m);

#endif /* FROSTD_METRICS_H */
