#include "prometheus.h"

#include <microhttpd.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- text format generation ---- */

static int find_sensor(prom_metrics_t *m, const char *sensor) {
    for (int i = 0; i < m->sensor_count; i++)
        if (strcmp(m->sensor_labels[i], sensor) == 0) return i;
    if (m->sensor_count >= PROM_MAX_SENSORS) return -1;
    strncpy(m->sensor_labels[m->sensor_count], sensor, PROM_MAX_NAME_LEN - 1);
    m->sensor_labels[m->sensor_count][PROM_MAX_NAME_LEN - 1] = '\0';
    return m->sensor_count++;
}

static int find_or_add_sensor_id(prom_metrics_t *m, int si, const char *id) {
    for (int i = 0; i < m->sensor_id_counts[si]; i++)
        if (strcmp(m->sensor_id_labels[si][i], id) == 0) return i;
    if (m->sensor_id_counts[si] >= PROM_MAX_IDS) return -1;
    int ii = m->sensor_id_counts[si]++;
    strncpy(m->sensor_id_labels[si][ii], id, PROM_MAX_ID_LEN - 1);
    m->sensor_id_labels[si][ii][PROM_MAX_ID_LEN - 1] = '\0';
    return ii;
}

static int find_fan(prom_metrics_t *m, const char *fan) {
    for (int i = 0; i < m->fan_count; i++)
        if (strcmp(m->fan_labels[i], fan) == 0) return i;
    if (m->fan_count >= PROM_MAX_FANS) return -1;
    strncpy(m->fan_labels[m->fan_count], fan, PROM_MAX_NAME_LEN - 1);
    m->fan_labels[m->fan_count][PROM_MAX_NAME_LEN - 1] = '\0';
    return m->fan_count++;
}

/* Generate Prometheus text format into a heap-allocated buffer.
 * Caller must free. */
static char *generate_text(prom_metrics_t *m) {
    char *buf = NULL;
    size_t sz = 0;
    FILE *fp = open_memstream(&buf, &sz);
    if (!fp) return NULL;

    pthread_mutex_lock(&m->lock);

    /* frostd_sensor_temperature */
    fputs("# HELP frostd_sensor_temperature Current temperature reading per sensor, in Celsius.\n"
          "# TYPE frostd_sensor_temperature gauge\n", fp);
    for (int si = 0; si < m->sensor_count; si++)
        for (int ii = 0; ii < m->sensor_id_counts[si]; ii++)
            fprintf(fp, "frostd_sensor_temperature{sensor=\"%s\",id=\"%s\"} %.2f\n",
                    m->sensor_labels[si], m->sensor_id_labels[si][ii],
                    m->sensor_temps[si][ii]);

    /* frostd_sensor_aggregate_temperature */
    fputs("# HELP frostd_sensor_aggregate_temperature Rolling aggregate temperature per sensor type, in Celsius.\n"
          "# TYPE frostd_sensor_aggregate_temperature gauge\n", fp);
    for (int si = 0; si < m->sensor_count; si++)
        for (int ii = 0; ii < m->sensor_id_counts[si]; ii++)
            fprintf(fp, "frostd_sensor_aggregate_temperature{sensor=\"%s\",id=\"%s\"} %.2f\n",
                    m->sensor_labels[si], m->sensor_id_labels[si][ii],
                    m->agg_temps[si][ii]);

    /* frostd_suggested_fan_speed_percent */
    fputs("# HELP frostd_suggested_fan_speed_percent Suggested fan speed percentage per sensor type.\n"
          "# TYPE frostd_suggested_fan_speed_percent gauge\n", fp);
    for (int si = 0; si < m->sensor_count; si++)
        fprintf(fp, "frostd_suggested_fan_speed_percent{sensor=\"%s\"} %.2f\n",
                m->sensor_labels[si], m->suggested_speeds[si]);

    /* frostd_actual_fan_rpm */
    fputs("# HELP frostd_actual_fan_rpm Actual fan speed in RPM.\n"
          "# TYPE frostd_actual_fan_rpm gauge\n", fp);
    for (int fi = 0; fi < m->fan_count; fi++)
        if (m->fan_has_rpm[fi])
            fprintf(fp, "frostd_actual_fan_rpm{fan=\"%s\"} %.2f\n",
                    m->fan_labels[fi], m->fan_rpm[fi]);

    /* frostd_actual_fan_speed_percent */
    fputs("# HELP frostd_actual_fan_speed_percent Actual fan speed as percentage.\n"
          "# TYPE frostd_actual_fan_speed_percent gauge\n", fp);
    for (int fi = 0; fi < m->fan_count; fi++)
        if (m->fan_has_pct[fi])
            fprintf(fp, "frostd_actual_fan_speed_percent{fan=\"%s\"} %.2f\n",
                    m->fan_labels[fi], m->fan_pct[fi]);

    /* frostd_system_fan_speed_percent */
    fputs("# HELP frostd_system_fan_speed_percent Commanded system fan speed percentage.\n"
          "# TYPE frostd_system_fan_speed_percent gauge\n", fp);
    fprintf(fp, "frostd_system_fan_speed_percent %.2f\n", m->system_speed);

    pthread_mutex_unlock(&m->lock);

    fclose(fp);
    return buf;
}

/* ---- libmicrohttpd request handler ---- */

static enum MHD_Result handle_request(void *cls,
                                       struct MHD_Connection *conn,
                                       const char *url,
                                       const char *method,
                                       const char *version,
                                       const char *upload_data,
                                       size_t *upload_data_size,
                                       void **con_cls) {
    (void)version; (void)upload_data; (void)upload_data_size; (void)con_cls;

    prom_metrics_t *m = cls;

    if (strcmp(url, "/metrics") != 0) {
        struct MHD_Response *r = MHD_create_response_from_buffer(
            9, (void *)"Not Found", MHD_RESPMEM_PERSISTENT);
        enum MHD_Result ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, r);
        MHD_destroy_response(r);
        return ret;
    }

    if (strcmp(method, "GET") != 0) {
        struct MHD_Response *r = MHD_create_response_from_buffer(
            0, (void *)"", MHD_RESPMEM_PERSISTENT);
        enum MHD_Result ret = MHD_queue_response(conn, MHD_HTTP_METHOD_NOT_ALLOWED, r);
        MHD_destroy_response(r);
        return ret;
    }

    char *body = generate_text(m);
    if (!body) return MHD_NO;

    struct MHD_Response *r = MHD_create_response_from_buffer(
        strlen(body), body, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(r, "Content-Type",
                            "text/plain; version=0.0.4; charset=utf-8");
    enum MHD_Result ret = MHD_queue_response(conn, MHD_HTTP_OK, r);
    MHD_destroy_response(r);
    return ret;
}

/* ---- public API ---- */

prom_metrics_t *prom_metrics_new(void) {
    prom_metrics_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

void prom_metrics_free(prom_metrics_t *m) {
    if (!m) return;
    prom_stop_server(m);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

int prom_start_server(prom_metrics_t *m, const char *addr,
                      logger_t *logger, char *errbuf, int errbuflen) {
    /* Parse "host:port" or ":port" */
    const char *colon = strrchr(addr, ':');
    unsigned short port = 9100;
    if (colon) port = (unsigned short)atoi(colon + 1);

    m->daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        port,
        NULL, NULL,
        handle_request, m,
        MHD_OPTION_END);

    if (!m->daemon) {
        snprintf(errbuf, errbuflen, "MHD_start_daemon failed on %s", addr);
        return -1;
    }
    log_info(logger, "prometheus metrics server listening",
             LOG_STR("addr", addr));
    return 0;
}

void prom_stop_server(prom_metrics_t *m) {
    if (m->daemon) {
        MHD_stop_daemon(m->daemon);
        m->daemon = NULL;
    }
}

void prom_set_sensor_temp(prom_metrics_t *m,
                           const char *sensor, const char *id, double temp) {
    pthread_mutex_lock(&m->lock);
    int si = find_sensor(m, sensor);
    if (si >= 0) {
        int ii = find_or_add_sensor_id(m, si, id);
        if (ii >= 0) m->sensor_temps[si][ii] = temp;
    }
    pthread_mutex_unlock(&m->lock);
}

void prom_set_agg_temp(prom_metrics_t *m,
                        const char *sensor, const char *id, double temp) {
    pthread_mutex_lock(&m->lock);
    int si = find_sensor(m, sensor);
    if (si >= 0) {
        int ii = find_or_add_sensor_id(m, si, id);
        if (ii >= 0) m->agg_temps[si][ii] = temp;
    }
    pthread_mutex_unlock(&m->lock);
}

void prom_set_suggested_speed(prom_metrics_t *m,
                               const char *sensor, double speed) {
    pthread_mutex_lock(&m->lock);
    int si = find_sensor(m, sensor);
    if (si >= 0) m->suggested_speeds[si] = speed;
    pthread_mutex_unlock(&m->lock);
}

void prom_set_fan_rpm(prom_metrics_t *m, const char *fan, double rpm) {
    pthread_mutex_lock(&m->lock);
    int fi = find_fan(m, fan);
    if (fi >= 0) { m->fan_rpm[fi] = rpm; m->fan_has_rpm[fi] = 1; }
    pthread_mutex_unlock(&m->lock);
}

void prom_set_fan_pct(prom_metrics_t *m, const char *fan, double pct) {
    pthread_mutex_lock(&m->lock);
    int fi = find_fan(m, fan);
    if (fi >= 0) { m->fan_pct[fi] = pct; m->fan_has_pct[fi] = 1; }
    pthread_mutex_unlock(&m->lock);
}

void prom_set_system_speed(prom_metrics_t *m, double speed) {
    pthread_mutex_lock(&m->lock);
    m->system_speed = speed;
    pthread_mutex_unlock(&m->lock);
}
