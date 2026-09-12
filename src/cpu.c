#include "cpu.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Entity ID 0x03 = Processor, per IPMI platform management spec.
 * libipmimonitoring doesn't expose entity IDs directly; we rely on the
 * sensor name to identify CPU packages. Dell PowerEdge CPU temperature
 * sensors are named "Temp" (single CPU) or "Temp" / "Temp_2" etc.
 * (multi-CPU), with entity ID 0x03. Non-CPU temp sensors (inlet, exhaust)
 * use entity IDs like 0x07 (system board), 0x40 (OEM).
 *
 * Since libipmimonitoring does not surface entity IDs in the iterator
 * API, we read all temperature sensors here and apply an allow-list filter
 * based on the sensor name patterns used by Dell PowerEdge 12th-gen BIOS.
 *
 * If in the future entity-ID filtering is needed, the record IDs approach
 * via ipmi_monitoring_sensor_readings_by_record_id can be substituted.
 */

/* Dell PowerEdge 12G CPU temp sensor name prefixes */
static const char *cpu_sensor_prefixes[] = {
    "Temp",
    "CPU",
    NULL,
};

static int is_cpu_sensor(const char *name) {
    for (int i = 0; cpu_sensor_prefixes[i]; i++) {
        if (strncmp(name, cpu_sensor_prefixes[i],
                    strlen(cpu_sensor_prefixes[i])) == 0)
            return 1;
    }
    return 0;
}

cpu_reader_t *cpu_reader_new(ipmi_ops_t *ops) {
    cpu_reader_t *r = malloc(sizeof(*r));
    if (!r) return NULL;
    r->ops = ops;
    return r;
}

void cpu_reader_free(cpu_reader_t *r) {
    free(r);
}

int cpu_read_temperatures(void *ctx,
                          char out_ids[][64], double *out_temps, int max,
                          char *errbuf, int errbuflen) {
    cpu_reader_t *r = ctx;

    if (r->ops->connect(r->ops->impl) != 0) {
        snprintf(errbuf, errbuflen, "connecting to IPMI: %s",
                 r->ops->last_error(r->ops->impl));
        return -1;
    }

    temp_sensor_raw_t raw[IPMI_MAX_SENSORS];
    int n = r->ops->get_temp_sensors(r->ops->impl, raw, IPMI_MAX_SENSORS);
    r->ops->close(r->ops->impl);

    if (n < 0) {
        snprintf(errbuf, errbuflen, "getting IPMI temperature sensors: %s",
                 r->ops->last_error(r->ops->impl));
        return -1;
    }

    int count = 0;
    for (int i = 0; i < n && count < max; i++) {
        if (!is_cpu_sensor(raw[i].name)) continue;

        /* Deduplicate: if name already used, append _2, _3, … */
        char key[64];
        strncpy(key, raw[i].name, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        for (int suffix = 2; ; suffix++) {
            int dup = 0;
            for (int j = 0; j < count; j++) {
                if (strcmp(out_ids[j], key) == 0) { dup = 1; break; }
            }
            if (!dup) break;
            snprintf(key, sizeof(key), "%.50s_%d", raw[i].name, suffix);
        }

        strncpy(out_ids[count], key, 64);
        out_ids[count][63] = '\0';
        out_temps[count] = raw[i].value;
        count++;
    }

    if (count == 0) {
        snprintf(errbuf, errbuflen, "no CPU temperature sensors found");
        return -1;
    }
    return count;
}
