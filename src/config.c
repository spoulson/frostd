#include "config.h"

#include <cyaml/cyaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- libcyaml schemas ---------- */

static const cyaml_schema_field_t prometheus_fields[] = {
    CYAML_FIELD_STRING_PTR("listen_addr", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           prometheus_config_t, listen_addr, 0, CYAML_UNLIMITED),
    CYAML_FIELD_END
};

static const cyaml_schema_field_t sensor_fields[] = {
    CYAML_FIELD_FLOAT("ideal_temp",      CYAML_FLAG_OPTIONAL, sensor_config_t, ideal_temp),
    CYAML_FIELD_FLOAT("max_temp",        CYAML_FLAG_OPTIONAL, sensor_config_t, max_temp),
    CYAML_FIELD_INT("sample_size",       CYAML_FLAG_OPTIONAL, sensor_config_t, sample_size),
    CYAML_FIELD_STRING_PTR("sample_interval",
                           CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           sensor_config_t, sample_interval, 0, CYAML_UNLIMITED),
    CYAML_FIELD_END
};

static const cyaml_schema_field_t config_fields[] = {
    CYAML_FIELD_STRING_PTR("log_file",
                           CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           frostd_config_t, log_file, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("log_format",
                           CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           frostd_config_t, log_format, 0, CYAML_UNLIMITED),
    CYAML_FIELD_BOOL("dry_run", CYAML_FLAG_OPTIONAL, frostd_config_t, dry_run),
    CYAML_FIELD_STRING_PTR("fan_log_interval",
                           CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           frostd_config_t, fan_log_interval, 0, CYAML_UNLIMITED),
    CYAML_FIELD_MAPPING_PTR("prometheus",
                            CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                            frostd_config_t, prometheus, prometheus_fields),
    CYAML_FIELD_MAPPING_PTR("cpu",
                            CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                            frostd_config_t, cpu, sensor_fields),
    CYAML_FIELD_MAPPING_PTR("gpu",
                            CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                            frostd_config_t, gpu, sensor_fields),
    CYAML_FIELD_END
};

static const cyaml_schema_value_t config_schema = {
    CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, frostd_config_t, config_fields)
};

static const cyaml_config_t cyaml_cfg = {
    .log_fn   = cyaml_log,
    .mem_fn   = cyaml_mem,
    .log_level = CYAML_LOG_WARNING,
};

/* ---------- duration parser ---------- */

int parse_duration(const char *s, double *seconds_out) {
    if (!s || !*s) return -1;

    char *end;
    double val = strtod(s, &end);
    if (end == s || val < 0) return -1;

    /* skip optional whitespace */
    while (isspace((unsigned char)*end)) end++;

    if (*end == '\0' || strcmp(end, "s") == 0) {
        *seconds_out = val;
    } else if (strcmp(end, "m") == 0) {
        *seconds_out = val * 60.0;
    } else if (strcmp(end, "h") == 0) {
        *seconds_out = val * 3600.0;
    } else if (strcmp(end, "ms") == 0) {
        *seconds_out = val / 1000.0;
    } else {
        return -1;
    }
    return 0;
}

/* ---------- defaults ---------- */

static void apply_sensor_defaults(sensor_config_t *s) {
    if (s->ideal_temp == 0.0) s->ideal_temp = 40.0;
    if (s->max_temp   == 0.0) s->max_temp   = 75.0;
    if (s->sample_size == 0)  s->sample_size = 3;
    if (!s->sample_interval) {
        s->sample_interval = strdup("15s");
        s->sample_interval_sec = 15.0;
    }
}

static int apply_defaults(frostd_config_t *cfg, char *errbuf, int errbuflen) {
    if (!cfg->log_format) cfg->log_format = strdup("text");

    if (!cfg->fan_log_interval) {
        cfg->fan_log_interval = strdup("15s");
        cfg->fan_log_interval_sec = 15.0;
    } else {
        if (parse_duration(cfg->fan_log_interval, &cfg->fan_log_interval_sec) != 0) {
            snprintf(errbuf, errbuflen,
                     "fan_log_interval: invalid duration %s", cfg->fan_log_interval);
            return -1;
        }
    }

    if (cfg->cpu) {
        apply_sensor_defaults(cfg->cpu);
        if (cfg->cpu->sample_interval_sec == 0.0) {
            if (parse_duration(cfg->cpu->sample_interval,
                               &cfg->cpu->sample_interval_sec) != 0) {
                snprintf(errbuf, errbuflen, "cpu.sample_interval: invalid duration %s",
                         cfg->cpu->sample_interval);
                return -1;
            }
        }
    }
    if (cfg->gpu) {
        apply_sensor_defaults(cfg->gpu);
        if (cfg->gpu->sample_interval_sec == 0.0) {
            if (parse_duration(cfg->gpu->sample_interval,
                               &cfg->gpu->sample_interval_sec) != 0) {
                snprintf(errbuf, errbuflen, "gpu.sample_interval: invalid duration %s",
                         cfg->gpu->sample_interval);
                return -1;
            }
        }
    }
    return 0;
}

/* ---------- validation ---------- */

static int validate_sensor(const char *name, const sensor_config_t *s,
                            char *errbuf, int errbuflen) {
    if (s->ideal_temp <= 0.0) {
        snprintf(errbuf, errbuflen, "%s: ideal_temp must be positive, got %.1f",
                 name, s->ideal_temp);
        return -1;
    }
    if (s->max_temp <= 0.0) {
        snprintf(errbuf, errbuflen, "%s: max_temp must be positive, got %.1f",
                 name, s->max_temp);
        return -1;
    }
    if (s->ideal_temp >= s->max_temp) {
        snprintf(errbuf, errbuflen,
                 "%s: ideal_temp (%.1f) must be less than max_temp (%.1f)",
                 name, s->ideal_temp, s->max_temp);
        return -1;
    }
    if (s->sample_size < 1) {
        snprintf(errbuf, errbuflen, "%s: sample_size must be at least 1, got %d",
                 name, s->sample_size);
        return -1;
    }
    if (s->sample_interval_sec <= 0.0) {
        snprintf(errbuf, errbuflen, "%s: sample_interval must be positive",
                 name);
        return -1;
    }
    return 0;
}

int config_validate(const frostd_config_t *cfg, char *errbuf, int errbuflen) {
    if (!cfg->cpu && !cfg->gpu) {
        snprintf(errbuf, errbuflen,
                 "config must enable at least one sensor type (cpu or gpu)");
        return -1;
    }
    if (strcmp(cfg->log_format, "text") != 0 &&
        strcmp(cfg->log_format, "json") != 0) {
        snprintf(errbuf, errbuflen,
                 "log_format must be \"text\" or \"json\", got \"%s\"",
                 cfg->log_format);
        return -1;
    }
    if (cfg->fan_log_interval_sec <= 0.0) {
        snprintf(errbuf, errbuflen, "fan_log_interval must be positive");
        return -1;
    }
    if (cfg->cpu && validate_sensor("cpu", cfg->cpu, errbuf, errbuflen) != 0)
        return -1;
    if (cfg->gpu && validate_sensor("gpu", cfg->gpu, errbuf, errbuflen) != 0)
        return -1;
    return 0;
}

/* ---------- public API ---------- */

int config_load(const char *path, frostd_config_t **cfg_out,
                char *errbuf, int errbuflen) {
    frostd_config_t *cfg = NULL;
    cyaml_err_t err = cyaml_load_file(path, &cyaml_cfg, &config_schema,
                                      (cyaml_data_t **)&cfg, NULL);
    if (err != CYAML_OK) {
        snprintf(errbuf, errbuflen, "parsing config file: %s",
                 cyaml_strerror(err));
        return -1;
    }

    if (apply_defaults(cfg, errbuf, errbuflen) != 0) {
        cyaml_free(&cyaml_cfg, &config_schema, cfg, 0);
        return -1;
    }
    if (config_validate(cfg, errbuf, errbuflen) != 0) {
        cyaml_free(&cyaml_cfg, &config_schema, cfg, 0);
        return -1;
    }

    *cfg_out = cfg;
    return 0;
}

void config_free(frostd_config_t *cfg) {
    if (!cfg) return;
    cyaml_free(&cyaml_cfg, &config_schema, cfg, 0);
}
