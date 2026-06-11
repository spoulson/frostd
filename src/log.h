#ifndef FROSTD_LOG_H
#define FROSTD_LOG_H

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;

typedef enum {
    LOG_FORMAT_TEXT,
    LOG_FORMAT_JSON,
} log_format_t;

typedef struct {
    FILE        *out;
    log_format_t format;
} logger_t;

/* Log field value types */
typedef enum {
    LOG_FIELD_STR,
    LOG_FIELD_INT,
    LOG_FIELD_FLOAT,
} log_field_type_t;

typedef struct {
    const char      *key;
    log_field_type_t type;
    union {
        const char *s;
        long long   i;
        double      f;
    } val;
} log_field_t;

#define LOG_STR(k, v)  ((log_field_t){(k), LOG_FIELD_STR,   {.s = (v)}})
#define LOG_INT(k, v)  ((log_field_t){(k), LOG_FIELD_INT,   {.i = (v)}})
#define LOG_FLOAT(k, v)((log_field_t){(k), LOG_FIELD_FLOAT, {.f = (v)}})
#define LOG_END        ((log_field_t){NULL, LOG_FIELD_STR,   {.s = NULL}})

logger_t *logger_new(FILE *out, log_format_t format);
void      logger_free(logger_t *l);

void logger_log(logger_t *l, log_level_t level, const char *msg,
                const log_field_t *fields, int nfields);

/* Convenience macros — __VA_OPT__(,) inserts comma only when fields given */
#define log_info(l, msg, ...) \
    do { \
        log_field_t _f[] = {__VA_ARGS__ __VA_OPT__(,) LOG_END}; \
        logger_log((l), LOG_LEVEL_INFO, (msg), _f, \
                   (int)(sizeof(_f)/sizeof(_f[0])) - 1); \
    } while (0)

#define log_warn(l, msg, ...) \
    do { \
        log_field_t _f[] = {__VA_ARGS__ __VA_OPT__(,) LOG_END}; \
        logger_log((l), LOG_LEVEL_WARN, (msg), _f, \
                   (int)(sizeof(_f)/sizeof(_f[0])) - 1); \
    } while (0)

#define log_error(l, msg, ...) \
    do { \
        log_field_t _f[] = {__VA_ARGS__ __VA_OPT__(,) LOG_END}; \
        logger_log((l), LOG_LEVEL_ERROR, (msg), _f, \
                   (int)(sizeof(_f)/sizeof(_f[0])) - 1); \
    } while (0)

#define log_debug(l, msg, ...) \
    do { \
        log_field_t _f[] = {__VA_ARGS__ __VA_OPT__(,) LOG_END}; \
        logger_log((l), LOG_LEVEL_DEBUG, (msg), _f, \
                   (int)(sizeof(_f)/sizeof(_f[0])) - 1); \
    } while (0)

#endif /* FROSTD_LOG_H */
