#include "metrics.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

sensor_monitor_t *sensor_monitor_new(const char *name, sensor_config_t *cfg,
                                     temp_reader_t reader) {
    sensor_monitor_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->name   = name;
    m->cfg    = cfg;
    m->reader = reader;
    return m;
}

void sensor_monitor_free(sensor_monitor_t *m) {
    if (!m) return;
    for (int i = 0; i < m->buf_count; i++)
        free(m->bufs[i].samples);
    free(m);
}

static sensor_buf_t *find_or_create_buf(sensor_monitor_t *m, const char *id) {
    for (int i = 0; i < m->buf_count; i++) {
        if (strcmp(m->bufs[i].id, id) == 0)
            return &m->bufs[i];
    }
    if (m->buf_count >= METRICS_MAX_SENSOR_IDS) return NULL;

    sensor_buf_t *b = &m->bufs[m->buf_count++];
    strncpy(b->id, id, sizeof(b->id) - 1);
    b->id[sizeof(b->id) - 1] = '\0';
    b->samples = calloc(m->cfg->sample_size, sizeof(double));
    b->head  = 0;
    b->count = 0;
    return b;
}

static void buf_push(sensor_buf_t *b, double temp, int sample_size) {
    b->samples[b->head] = temp;
    b->head = (b->head + 1) % sample_size;
    if (b->count < sample_size) b->count++;
}

static double buf_mean(const sensor_buf_t *b, int sample_size) {
    if (b->count == 0) return 0.0;
    double sum = 0.0;
    /* Walk backwards from head - count to head */
    int start = (b->head - b->count + sample_size) % sample_size;
    for (int i = 0; i < b->count; i++)
        sum += b->samples[(start + i) % sample_size];
    return sum / (double)b->count;
}

int sensor_monitor_poll(sensor_monitor_t *m,
                        char out_ids[][64], double *out_temps,
                        double *out_aggs, int max,
                        char *errbuf, int errbuflen) {
    char   raw_ids[METRICS_MAX_SENSOR_IDS][64];
    double raw_temps[METRICS_MAX_SENSOR_IDS];

    int n = m->reader.read_temperatures(m->reader.ctx,
                                         raw_ids, raw_temps,
                                         METRICS_MAX_SENSOR_IDS,
                                         errbuf, errbuflen);
    if (n < 0) return -1;
    if (n == 0) {
        snprintf(errbuf, errbuflen, "%s: no temperature readings returned", m->name);
        return -1;
    }

    int out_count = (n < max) ? n : max;
    for (int i = 0; i < out_count; i++) {
        sensor_buf_t *b = find_or_create_buf(m, raw_ids[i]);
        if (!b) {
            snprintf(errbuf, errbuflen, "%s: too many sensor IDs", m->name);
            return -1;
        }
        buf_push(b, raw_temps[i], m->cfg->sample_size);

        strncpy(out_ids[i], raw_ids[i], 64);
        out_ids[i][63] = '\0';
        out_temps[i] = raw_temps[i];
        out_aggs[i]  = buf_mean(b, m->cfg->sample_size);
    }
    return out_count;
}

int sensor_monitor_aggregates(const sensor_monitor_t *m,
                               char out_ids[][64], double *out_aggs, int max) {
    int count = (m->buf_count < max) ? m->buf_count : max;
    for (int i = 0; i < count; i++) {
        strncpy(out_ids[i], m->bufs[i].id, 64);
        out_ids[i][63] = '\0';
        out_aggs[i] = buf_mean(&m->bufs[i], m->cfg->sample_size);
    }
    return count;
}
