#ifndef FROSTD_PROMETHEUS_H
#define FROSTD_PROMETHEUS_H

#include "log.h"
#include <pthread.h>

#define PROM_MAX_SENSORS  4
#define PROM_MAX_IDS      32
#define PROM_MAX_FANS     32
#define PROM_MAX_ID_LEN   64
#define PROM_MAX_NAME_LEN 64

typedef struct {
    /* Sensor temperature: frostd_sensor_temperature{sensor, id} */
    char   sensor_labels[PROM_MAX_SENSORS][PROM_MAX_NAME_LEN];
    char   sensor_id_labels[PROM_MAX_SENSORS][PROM_MAX_IDS][PROM_MAX_ID_LEN];
    double sensor_temps[PROM_MAX_SENSORS][PROM_MAX_IDS];
    int    sensor_id_counts[PROM_MAX_SENSORS];
    int    sensor_count;

    /* Aggregate temperature: frostd_sensor_aggregate_temperature{sensor, id} */
    double agg_temps[PROM_MAX_SENSORS][PROM_MAX_IDS];

    /* Suggested speed: frostd_suggested_fan_speed_percent{sensor} */
    double suggested_speeds[PROM_MAX_SENSORS];

    /* Fan readings: frostd_actual_fan_rpm{fan}, frostd_actual_fan_speed_percent{fan} */
    char   fan_labels[PROM_MAX_FANS][PROM_MAX_NAME_LEN];
    double fan_rpm[PROM_MAX_FANS];
    int    fan_has_rpm[PROM_MAX_FANS];
    double fan_pct[PROM_MAX_FANS];
    int    fan_has_pct[PROM_MAX_FANS];
    int    fan_count;

    /* System speed: frostd_system_fan_speed_percent */
    double system_speed;

    pthread_mutex_t lock;
    struct MHD_Daemon *daemon;
} prom_metrics_t;

/* Allocate and initialise a prom_metrics_t. */
prom_metrics_t *prom_metrics_new(void);

/* Free. Stops the HTTP server if running. */
void prom_metrics_free(prom_metrics_t *m);

/* Start the libmicrohttpd server on addr (e.g. ":9100"). Returns 0 on success. */
int prom_start_server(prom_metrics_t *m, const char *addr,
                      logger_t *logger, char *errbuf, int errbuflen);

/* Stop the HTTP server. */
void prom_stop_server(prom_metrics_t *m);

/* Thread-safe update functions */
void prom_set_sensor_temp(prom_metrics_t *m,
                           const char *sensor, const char *id, double temp);
void prom_set_agg_temp(prom_metrics_t *m,
                        const char *sensor, const char *id, double temp);
void prom_set_suggested_speed(prom_metrics_t *m,
                               const char *sensor, double speed);
void prom_set_fan_rpm(prom_metrics_t *m, const char *fan, double rpm);
void prom_set_fan_pct(prom_metrics_t *m, const char *fan, double pct);
void prom_set_system_speed(prom_metrics_t *m, double speed);

#endif /* FROSTD_PROMETHEUS_H */
