#ifndef FROSTD_CPU_H
#define FROSTD_CPU_H

#include "ipmi.h"
#include "metrics.h"

/*
 * CPU temperature reader backed by IPMI.
 * Reads temperature sensors with entity ID 0x03 (processor).
 */
typedef struct {
    ipmi_ops_t *ops;
} cpu_reader_t;

cpu_reader_t *cpu_reader_new(ipmi_ops_t *ops);
void          cpu_reader_free(cpu_reader_t *r);

/* temp_reader_t callback — use cpu_reader_make_reader() to wrap */
int cpu_read_temperatures(void *ctx,
                          char out_ids[][64], double *out_temps, int max,
                          char *errbuf, int errbuflen);

static inline temp_reader_t cpu_reader_make_reader(cpu_reader_t *r) {
    return (temp_reader_t){cpu_read_temperatures, r};
}

#endif /* FROSTD_CPU_H */
