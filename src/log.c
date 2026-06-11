#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

logger_t *logger_new(FILE *out, log_format_t format) {
    logger_t *l = malloc(sizeof(*l));
    if (!l) return NULL;
    l->out    = out;
    l->format = format;
    return l;
}

void logger_free(logger_t *l) {
    free(l);
}

static const char *level_str(log_level_t level) {
    switch (level) {
    case LOG_LEVEL_DEBUG: return "DEBUG";
    case LOG_LEVEL_INFO:  return "INFO";
    case LOG_LEVEL_WARN:  return "WARN";
    case LOG_LEVEL_ERROR: return "ERROR";
    }
    return "INFO";
}

static void iso8601_now(char *buf, size_t len) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);
    char tmp[32];
    strftime(tmp, sizeof(tmp), "%Y-%m-%dT%H:%M:%S", &tm);
    snprintf(buf, len, "%s.%03ldZ", tmp, ts.tv_nsec / 1000000L);
}

static void write_json_str(FILE *out, const char *s) {
    fputc('"', out);
    for (; *s; s++) {
        switch (*s) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n",  out); break;
        case '\r': fputs("\\r",  out); break;
        case '\t': fputs("\\t",  out); break;
        default:   fputc(*s, out);     break;
        }
    }
    fputc('"', out);
}

static void write_text(logger_t *l, log_level_t level, const char *msg,
                       const log_field_t *fields, int nfields) {
    char ts[40];
    iso8601_now(ts, sizeof(ts));
    fprintf(l->out, "time=%s level=%s msg=%s", ts, level_str(level), msg);
    for (int i = 0; i < nfields; i++) {
        switch (fields[i].type) {
        case LOG_FIELD_STR:
            fprintf(l->out, " %s=%s", fields[i].key, fields[i].val.s);
            break;
        case LOG_FIELD_INT:
            fprintf(l->out, " %s=%lld", fields[i].key, fields[i].val.i);
            break;
        case LOG_FIELD_FLOAT:
            fprintf(l->out, " %s=%.2f", fields[i].key, fields[i].val.f);
            break;
        }
    }
    fputc('\n', l->out);
}

static void write_json(logger_t *l, log_level_t level, const char *msg,
                       const log_field_t *fields, int nfields) {
    char ts[40];
    iso8601_now(ts, sizeof(ts));
    fprintf(l->out, "{\"time\":\"%s\",\"level\":\"%s\",\"msg\":", ts, level_str(level));
    write_json_str(l->out, msg);
    for (int i = 0; i < nfields; i++) {
        fprintf(l->out, ",\"%s\":", fields[i].key);
        switch (fields[i].type) {
        case LOG_FIELD_STR:
            write_json_str(l->out, fields[i].val.s);
            break;
        case LOG_FIELD_INT:
            fprintf(l->out, "%lld", fields[i].val.i);
            break;
        case LOG_FIELD_FLOAT:
            fprintf(l->out, "%.2f", fields[i].val.f);
            break;
        }
    }
    fputs("}\n", l->out);
}

void logger_log(logger_t *l, log_level_t level, const char *msg,
                const log_field_t *fields, int nfields) {
    if (!l) return;
    if (l->format == LOG_FORMAT_JSON)
        write_json(l, level, msg, fields, nfields);
    else
        write_text(l, level, msg, fields, nfields);
    fflush(l->out);
}
