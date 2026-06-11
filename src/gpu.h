#ifndef FROSTD_GPU_H
#define FROSTD_GPU_H

#include "metrics.h"

/*
 * Command runner vtable — analogous to Go's CommandRunner interface.
 * run() executes a command and writes stdout into buf (null-terminated).
 * Returns number of bytes written, or -1 on error.
 */
typedef struct {
    int (*run)(void *ctx, const char *prog, const char *const *args,
               char *buf, int buflen, char *errbuf, int errbuflen);
    void *ctx;
} cmd_runner_t;

typedef struct {
    cmd_runner_t runner;
} gpu_reader_t;

/*
 * Parse nvidia-smi CSV output (one temperature per line).
 * out_ids must be char[max][64], out_temps double[max].
 * Returns number of GPUs parsed, or -1 on error.
 */
int parse_gpu_temps(const char *output,
                    char out_ids[][64], double *out_temps, int max,
                    char *errbuf, int errbuflen);

/*
 * Build a temp_reader_t backed by nvidia-smi via a cmd_runner_t.
 * The returned reader's ctx points to a heap-allocated gpu_reader_t.
 * Free with gpu_reader_free().
 */
gpu_reader_t *gpu_reader_new(cmd_runner_t runner);
void          gpu_reader_free(gpu_reader_t *r);

/* temp_reader_t read_temperatures callback — used via gpu_reader_make_reader() */
int gpu_read_temperatures(void *ctx,
                          char out_ids[][64], double *out_temps, int max,
                          char *errbuf, int errbuflen);

/* Returns a temp_reader_t wired to a gpu_reader_t */
static inline temp_reader_t gpu_reader_make_reader(gpu_reader_t *r) {
    return (temp_reader_t){gpu_read_temperatures, r};
}

/* Real popen-based command runner (used in production) */
cmd_runner_t cmd_runner_real(void);

#endif /* FROSTD_GPU_H */
