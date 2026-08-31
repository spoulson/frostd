#include "config.h"
#include "cpu.h"
#include "fan.h"
#include "gpu.h"
#include "ipmi.h"
#include "log.h"
#include "metrics.h"
#include "prometheus.h"
#include "service.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

#define VERSION "0.1.0"

volatile sig_atomic_t g_shutdown = 0;

static void sig_handler(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static void setup_signals(void) {
    struct sigaction sa = {.sa_handler = sig_handler};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
}

static logger_t *setup_logger(const char *log_file, const char *log_format,
                               char *errbuf, int errbuflen) {
    FILE *out = stdout;
    if (log_file && *log_file) {
        /* mkdir -p the parent directory */
        char *tmp = strdup(log_file);
        char *dir = dirname(tmp);
        struct stat st;
        if (stat(dir, &st) != 0) {
            if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
                snprintf(errbuf, errbuflen, "mkdir %s: %s", dir, strerror(errno));
                free(tmp);
                return NULL;
            }
        }
        free(tmp);

        out = fopen(log_file, "a");
        if (!out) {
            snprintf(errbuf, errbuflen, "open %s: %s", log_file, strerror(errno));
            return NULL;
        }
    }

    log_format_t fmt = LOG_FORMAT_TEXT;
    if (log_format && strcmp(log_format, "json") == 0)
        fmt = LOG_FORMAT_JSON;

    return logger_new(out, fmt);
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-c config] [-version]\n", prog);
}

int main(int argc, char *argv[]) {
    const char *config_path = "/etc/frostd.yaml";
    int show_version = 0;

    /* Pre-scan for -version / --version before getopt consumes the args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-version") == 0 ||
            strcmp(argv[i], "--version") == 0) {
            show_version = 1;
        }
    }

    opterr = 0; /* suppress getopt error messages; we handle them below */
    int opt;
    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
        case 'c':
            config_path = optarg;
            break;
        case '?':
            if (!show_version) {
                print_usage(argv[0]);
                return 1;
            }
            break;
        default:
            break;
        }
    }

    if (show_version) {
        printf("%s\n", VERSION);
        return 0;
    }

    /* Load configuration */
    frostd_config_t *cfg = NULL;
    char errbuf[512];
    if (config_load(config_path, &cfg, errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "frostd: configuration error: %s\n", errbuf);
        return 1;
    }

    /* Setup logger */
    logger_t *logger = setup_logger(cfg->log_file, cfg->log_format,
                                    errbuf, sizeof(errbuf));
    if (!logger) {
        fprintf(stderr, "frostd: failed to open log file: %s\n", errbuf);
        config_free(cfg);
        return 1;
    }

    /* Setup IPMI */
    ipmi_ops_t *ipmi = ipmi_ops_new_real();
    if (!ipmi) {
        fprintf(stderr, "frostd: failed to initialise IPMI\n");
        return 1;
    }

    /* Build service */
    service_t svc = {
        .cfg         = cfg,
        .fan_ctrl    = {.ops = ipmi},
        .monitor_count = 0,
        .logger      = logger,
    };

    /* Build sensor monitors */
    if (cfg->cpu) {
        cpu_reader_t *cr = cpu_reader_new(ipmi_ops_new_real());
        temp_reader_t tr = cpu_reader_make_reader(cr);
        svc.monitors[svc.monitor_count++] =
            sensor_monitor_new("cpu", cfg->cpu, tr);
    }
    if (cfg->gpu) {
        gpu_reader_t *gr = gpu_reader_new(cmd_runner_real());
        temp_reader_t tr = gpu_reader_make_reader(gr);
        svc.monitors[svc.monitor_count++] =
            sensor_monitor_new("gpu", cfg->gpu, tr);
    }

    /* Setup Prometheus */
    prom_metrics_t *prom = prom_metrics_new();
    svc.prom = prom;

    if (cfg->prometheus && cfg->prometheus->listen_addr &&
        *cfg->prometheus->listen_addr) {
        if (prom_start_server(prom, cfg->prometheus->listen_addr,
                              logger, errbuf, sizeof(errbuf)) != 0) {
            log_error(logger, "failed to start prometheus server",
                      LOG_STR("error", errbuf));
        }
    }

    /* Install signal handlers */
    setup_signals();

    log_info(logger, "frostd started",
             LOG_STR("config",   config_path),
             LOG_INT("dry_run",  cfg->dry_run ? 1 : 0));

    service_run(&svc);

    log_info(logger, "frostd stopping");

    /* Cleanup */
    prom_metrics_free(prom);
    for (int i = 0; i < svc.monitor_count; i++)
        sensor_monitor_free(svc.monitors[i]);
    ipmi_ops_free(ipmi);
    config_free(cfg);
    logger_free(logger);
    return 0;
}
