#include "gpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- popen-based real runner ---- */

static int real_run(void *ctx,
                    const char *prog, const char *const *args,
                    char *buf, int buflen, char *errbuf, int errbuflen) {
    (void)ctx;

    /* Build the command string */
    char cmd[1024] = {0};
    int  pos = 0;
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, "%s", prog);
    for (int i = 0; args && args[i]; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", args[i]);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(errbuf, errbuflen, "popen(%s) failed", prog);
        return -1;
    }

    int n = (int)fread(buf, 1, buflen - 1, fp);
    buf[n < 0 ? 0 : n] = '\0';
    int rc = pclose(fp);
    if (rc != 0) {
        snprintf(errbuf, errbuflen, "%s exited with status %d", prog, rc);
        return -1;
    }
    return n;
}

cmd_runner_t cmd_runner_real(void) {
    return (cmd_runner_t){real_run, NULL};
}

/* ---- parse_gpu_temps ---- */

int parse_gpu_temps(const char *output,
                    char out_ids[][64], double *out_temps, int max,
                    char *errbuf, int errbuflen) {
    if (!output || !*output) {
        snprintf(errbuf, errbuflen, "no GPU temperatures found in nvidia-smi output");
        return -1;
    }

    int count = 0;
    const char *p = output;

    while (*p && count < max) {
        /* skip leading whitespace / blank lines */
        while (*p && (isspace((unsigned char)*p)) && *p != '\n') p++;
        if (*p == '\n') { p++; continue; }
        if (!*p) break;

        /* read one number */
        char *end;
        double temp = strtod(p, &end);
        if (end == p) {
            snprintf(errbuf, errbuflen, "parsing GPU temperature: unexpected token");
            return -1;
        }

        /* advance to end of line */
        while (*end && *end != '\n') end++;
        if (*end == '\n') end++;

        /* check trailing junk on the same line was just whitespace */
        const char *check = p + (end - p);
        (void)check;

        if (count == 0) {
            strncpy(out_ids[0], "Temp", 64);
        } else {
            snprintf(out_ids[count], 64, "Temp_%d", count + 1);
        }
        out_temps[count] = temp;
        count++;
        p = end;
    }

    if (count == 0) {
        snprintf(errbuf, errbuflen, "no GPU temperatures found in nvidia-smi output");
        return -1;
    }
    return count;
}

/* ---- gpu_reader_t ---- */

gpu_reader_t *gpu_reader_new(cmd_runner_t runner) {
    gpu_reader_t *r = malloc(sizeof(*r));
    if (!r) return NULL;
    r->runner = runner;
    return r;
}

void gpu_reader_free(gpu_reader_t *r) {
    free(r);
}

int gpu_read_temperatures(void *ctx,
                          char out_ids[][64], double *out_temps, int max,
                          char *errbuf, int errbuflen) {
    gpu_reader_t *r = ctx;
    char buf[4096];
    const char *args[] = {
        "--query-gpu=temperature.gpu",
        "--format=csv,noheader",
        NULL,
    };
    int n = r->runner.run(r->runner.ctx, "nvidia-smi", args,
                          buf, sizeof(buf), errbuf, errbuflen);
    if (n < 0) return -1;
    return parse_gpu_temps(buf, out_ids, out_temps, max, errbuf, errbuflen);
}
