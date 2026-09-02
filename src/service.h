#ifndef FROSTD_SERVICE_H
#define FROSTD_SERVICE_H

#include "config.h"
#include "fan.h"
#include "metrics.h"
#include "prometheus.h"
#include "log.h"

#include <pthread.h>
#include <stdatomic.h>

#define SERVICE_MAX_MONITORS 8

/* Speed bus: sensor threads post speed suggestions here; the main loop reads them. */
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    int             speeds[SERVICE_MAX_MONITORS];
    int             updated;
    int             stop;
} speed_bus_t;

typedef struct {
    const char      *sensor_name; /* "cpu" or "gpu" */
    sensor_monitor_t *monitor;
    speed_bus_t      *bus;
    int               sensor_idx;
    logger_t         *logger;
    prom_metrics_t   *prom;
} sensor_thread_arg_t;

typedef struct {
    frostd_config_t  *cfg;
    fan_controller_t  fan_ctrl;
    sensor_monitor_t *monitors[SERVICE_MAX_MONITORS];
    int               monitor_count;
    logger_t         *logger;
    prom_metrics_t   *prom;

    speed_bus_t       bus;
    pthread_t         sensor_threads[SERVICE_MAX_MONITORS];
    sensor_thread_arg_t sensor_args[SERVICE_MAX_MONITORS];
} service_t;

/*
 * Run the service until g_shutdown is set (SIGTERM/SIGINT).
 * Spawns one sensor thread per monitor, then runs the main loop.
 */
void service_run(service_t *svc);

#endif /* FROSTD_SERVICE_H */
