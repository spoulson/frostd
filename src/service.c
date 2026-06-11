#include "service.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>

#include <signal.h>

/* Set by sigaction handler in main.c */
extern volatile sig_atomic_t g_shutdown;

/* ---- helpers ---- */

static void timespec_add_sec(struct timespec *ts, double seconds) {
    long ns = (long)(seconds * 1e9);
    ts->tv_sec  += ns / 1000000000L;
    ts->tv_nsec += ns % 1000000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

/* ---- sensor thread ---- */

static void *sensor_thread(void *arg) {
    sensor_thread_arg_t *a   = arg;
    sensor_monitor_t    *m   = a->monitor;
    speed_bus_t         *bus = a->bus;
    logger_t            *log = a->logger;
    prom_metrics_t      *prom = a->prom;

    struct timespec next_tick;
    clock_gettime(CLOCK_MONOTONIC, &next_tick);

    while (!g_shutdown) {
        timespec_add_sec(&next_tick, m->cfg->sample_interval_sec);

        char ids[METRICS_MAX_SENSOR_IDS][64];
        double temps[METRICS_MAX_SENSOR_IDS], aggs[METRICS_MAX_SENSOR_IDS];
        char errbuf[256];
        int n = sensor_monitor_poll(m, ids, temps, aggs,
                                    METRICS_MAX_SENSOR_IDS, errbuf, sizeof(errbuf));
        if (n < 0) {
            log_error(log, "sensor temperature poll failed",
                      LOG_STR("sensor", a->sensor_name),
                      LOG_STR("error", errbuf));
        } else {
            /* Find max aggregate for speed suggestion */
            double max_agg = 0.0;
            for (int i = 0; i < n; i++)
                if (aggs[i] > max_agg) max_agg = aggs[i];

            int speed = suggest_speed(max_agg,
                                       m->cfg->ideal_temp, m->cfg->max_temp);

            log_info(log, "sensor suggested fan speed",
                     LOG_STR("sensor", a->sensor_name),
                     LOG_INT("suggest_percent", speed));

            /* Update Prometheus */
            if (prom) {
                for (int i = 0; i < n; i++) {
                    prom_set_sensor_temp(prom, a->sensor_name, ids[i], temps[i]);
                    prom_set_agg_temp(prom, a->sensor_name, ids[i], aggs[i]);
                }
                prom_set_suggested_speed(prom, a->sensor_name, (double)speed);
            }

            /* Post speed to bus */
            pthread_mutex_lock(&bus->lock);
            bus->speeds[a->sensor_idx] = speed;
            bus->updated = 1;
            pthread_cond_signal(&bus->cond);
            pthread_mutex_unlock(&bus->lock);
        }

        /* Sleep until next_tick */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec < next_tick.tv_sec ||
            (now.tv_sec == next_tick.tv_sec && now.tv_nsec < next_tick.tv_nsec)) {
            while (!g_shutdown) {
                int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                          &next_tick, NULL);
                if (rc == 0 || rc == EINTR) break;
            }
        }
    }
    return NULL;
}

/* ---- log fan speeds ---- */

static void log_fan_speeds(service_t *svc,
                            fan_reading_t *prev, int *prev_count) {
    fan_reading_t readings[PROM_MAX_FANS];
    char errbuf[256];
    int n = fan_read_speeds(&svc->fan_ctrl, readings, PROM_MAX_FANS,
                             errbuf, sizeof(errbuf));
    if (n < 0) {
        log_error(svc->logger, "failed to read current fan speeds",
                  LOG_STR("error", errbuf));
        return;
    }

    for (int i = 0; i < n; i++) {
        if (readings[i].rpm.valid) {
            /* Find prev reading for delta */
            int delta_rpm_valid = 0;
            double delta_rpm = 0.0;
            for (int j = 0; j < *prev_count; j++) {
                if (strcmp(prev[j].name, readings[i].name) == 0 && prev[j].rpm.valid) {
                    delta_rpm = readings[i].rpm.value - prev[j].rpm.value;
                    delta_rpm_valid = 1;
                    break;
                }
            }
            if (delta_rpm_valid) {
                log_info(svc->logger, "current fan speed",
                         LOG_STR("fan", readings[i].name),
                         LOG_INT("rpm",   (long long)readings[i].rpm.value),
                         LOG_INT("delta", (long long)delta_rpm));
            } else {
                log_info(svc->logger, "current fan speed",
                         LOG_STR("fan", readings[i].name),
                         LOG_INT("rpm", (long long)readings[i].rpm.value));
            }
            if (svc->prom)
                prom_set_fan_rpm(svc->prom, readings[i].name, readings[i].rpm.value);
        }
        if (readings[i].percent.valid) {
            if (svc->prom)
                prom_set_fan_pct(svc->prom, readings[i].name, readings[i].percent.value);
        }
    }

    /* Update prev readings */
    if (n <= PROM_MAX_FANS) {
        memcpy(prev, readings, n * sizeof(*readings));
        *prev_count = n;
    }
}

/* ---- main service loop ---- */

void service_run(service_t *svc) {
    pthread_mutex_init(&svc->bus.lock, NULL);
    pthread_cond_init(&svc->bus.cond, NULL);
    svc->bus.stop    = 0;
    svc->bus.updated = 0;
    for (int i = 0; i < svc->monitor_count; i++)
        svc->bus.speeds[i] = 0;

    /* Start sensor threads */
    for (int i = 0; i < svc->monitor_count; i++) {
        svc->sensor_args[i] = (sensor_thread_arg_t){
            .sensor_name = svc->monitors[i]->name,
            .monitor     = svc->monitors[i],
            .bus         = &svc->bus,
            .sensor_idx  = i,
            .logger      = svc->logger,
            .prom        = svc->prom,
        };
        pthread_create(&svc->sensor_threads[i], NULL,
                       sensor_thread, &svc->sensor_args[i]);
    }

    /* Log initial fan speeds */
    fan_reading_t prev_fans[PROM_MAX_FANS] = {0};
    int prev_fan_count = 0;
    log_fan_speeds(svc, prev_fans, &prev_fan_count);

    int last_speed  = -1;
    int pending     = 0;

    /* Compute absolute deadline for next fan log */
    struct timespec fan_log_deadline;
    clock_gettime(CLOCK_MONOTONIC, &fan_log_deadline);
    timespec_add_sec(&fan_log_deadline, svc->cfg->fan_log_interval_sec);

    while (!g_shutdown) {
        /* Wait for a speed update or fan-log timeout */
        pthread_mutex_lock(&svc->bus.lock);
        while (!svc->bus.updated && !g_shutdown) {
            int rc = pthread_cond_timedwait(&svc->bus.cond, &svc->bus.lock,
                                             &fan_log_deadline);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&svc->bus.lock);
                log_fan_speeds(svc, prev_fans, &prev_fan_count);
                timespec_add_sec(&fan_log_deadline, svc->cfg->fan_log_interval_sec);
                pthread_mutex_lock(&svc->bus.lock);
            }
        }
        if (svc->bus.updated) {
            /* Compute max speed across all sensors */
            int max_speed = 0;
            for (int i = 0; i < svc->monitor_count; i++)
                if (svc->bus.speeds[i] > max_speed)
                    max_speed = svc->bus.speeds[i];
            svc->bus.updated = 0;
            pending = 1;
            pthread_mutex_unlock(&svc->bus.lock);

            if (max_speed != last_speed) {
                last_speed = max_speed;
                if (svc->prom)
                    prom_set_system_speed(svc->prom, (double)max_speed);

                if (svc->cfg->dry_run) {
                    log_info(svc->logger, "dry run: skipping fan speed change",
                             LOG_INT("system_percent", max_speed));
                } else {
                    log_info(svc->logger, "setting fan speed",
                             LOG_INT("system_percent", max_speed));
                    char errbuf[256];
                    if (fan_set_speed(&svc->fan_ctrl, max_speed,
                                      errbuf, sizeof(errbuf)) != 0) {
                        log_error(svc->logger, "failed to set fan speed",
                                  LOG_STR("error", errbuf));
                    }
                }
            }
        } else {
            pthread_mutex_unlock(&svc->bus.lock);
        }
        (void)pending; /* suppress unused warning; pattern matches Go logic */
        pending = 0;
    }

    /* Signal sensor threads to stop and wait */
    pthread_mutex_lock(&svc->bus.lock);
    svc->bus.stop = 1;
    pthread_cond_broadcast(&svc->bus.cond);
    pthread_mutex_unlock(&svc->bus.lock);

    for (int i = 0; i < svc->monitor_count; i++)
        pthread_join(svc->sensor_threads[i], NULL);

    pthread_cond_destroy(&svc->bus.cond);
    pthread_mutex_destroy(&svc->bus.lock);
}
