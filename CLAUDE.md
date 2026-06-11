# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project

`frostd` is a daemon that dynamically regulates fan speeds on Dell PowerEdge
servers based on hardware temperature metrics.

`frostd` supports Dell PowerEdge 12th generation servers, such as R720.

`frostd` identifies supported devices and actively monitors metrics and adjusts
chassis fan speed using IPMI.

## Sensor Metrics

Devices are grouped by sensor type.  Each sensor type has zero or more sensor
IDs.

### CPU Sensor Metrics

`frostd` uses IPMI to read CPU temperatures of each package.  Each package is
listed as a sensor ID.

### NVIDIA Tesla P40 GPU Sensor Metrics

`frostd` supports monitoring of an optional NVIDIA Tesla P40 GPU.  frostd uses
`nvidia-smi` command to read GPU temperature.

#### System Prerequisites

- Compatible NVIDIA driver installed
- Compatible Cuda driver installed
  - This installs the `nvidia-smi` command

## Fan Speed Control

frostd computes a suggested fan speed in range [0,100] based on configurable
ideal and maximum device temperatures and an easing formula.

### Suggestion Algorithm

For each sensor type, read the temperature and compute aggregate temperature
metric from average of last _n_ samples.

The default easing type is "parabolic".  Compute the suggested fan speed for
each device with the following pseudocode formula: 

```
suggested_speed = min(
    max_speed,
    (actual_temp - ideal_temp)^2 * (max_temp/((max_temp - ideal_temp)^2))
) 
```

Design assumptions:
- Temperature unit is Celcius.
- `max_speed` = 100.
- `max_temp` is the upper bound temperature at which fans will be run at
highest speed.
- `ideal_temp` is the lower bound temperature at which fans will be run at
lowest speed.
- `max_temp` and `ideal_temp` are configured per device.
- `actual_temp` is the sensor aggregate temperature metric.
- Set fan speed to the max `suggested_speed` computed from all devices. This
ensures adequate airflow for the device that needs it most.

## Configuration

frostd reads a YAML config file in the default path `/etc/frostd.yaml` or
overridden by specifying CLI argument `-c <file>`.

The YAML config specifies which sensor types to monitor: CPU and/or NVIDIA
Tesla P40 GPU.  Each sensor type includes settings for:
- ideal temperature in Celcius (Default 40)
- maximum temperature in Celcius (Default 75)
- sample size (Default 3)
- sample time interval (Default 15s)

All instances of a sensor type will be discovered (e.g. multiple CPU packages,
multiple GPUs).

Log file output file is specified (Default `/var/log/frostd/frostd.log`).

Dry run mode is enabled with `dry_run: true` (Default `false`).  When enabled,
frostd monitors temperature metrics and logs suggested fan speeds but does not
issue any fan speed changes via IPMI.  Useful for observing behavior without
affecting hardware.

## Technology Stack

`frostd` is written in C (C23 standard) and compiled to a standalone executable
to be run on Debian or Ubuntu Linux on x86-64 architecture.  The executable
runs as a systemd service.

## Developer Prerequisites

- Debian or Ubuntu Linux OS on x86-64 architecture
- GCC 9+ installed (GCC 13+ recommended for C23 support)
- `make` installed
- C development libraries:
  - `libcyaml-dev` (YAML parsing)
  - `libyaml-dev` (libcyaml dependency)
  - `libipmimonitoring-dev` (IPMI sensor reads)
  - `libfreeipmi-dev` (IPMI raw commands)
  - `libmicrohttpd-dev` (Prometheus HTTP server)
  - `libcmocka-dev` (unit testing)
  - `cppcheck` (static analysis)

## Software Design

### Technology Stack

- `frostd` is a systemd service written in C (C23 standard).
- It must be run on a Dell PowerEdge 12th generation server.

### Source Layout

```
src/        C source and header files
tests/      Unit test files and shared test helpers
packaging/  Debian package scripts and systemd configuration
```

### Tooling

- Use structured logging via `src/log.c/.h` (text key=value or JSON format).
- Use `libipmimonitoring` for IPMI sensor reads (temperature, fan).
- Use `libfreeipmi` API (`ipmi_ctx_t`) for IPMI raw commands (fan control).
- Do not depend on the commonly used `ipmitool` command.
- Use `libcyaml` for YAML config parsing.
- Use `libmicrohttpd` for the Prometheus metrics HTTP endpoint.

### Interfaces (vtable pattern)

C interfaces are implemented as function-pointer structs:
- `ipmi_ops_t` (`src/ipmi.h`) — IPMI operations (connect/sensors/raw command)
- `temp_reader_t` (`src/metrics.h`) — temperature reader for sensor monitors
- `cmd_runner_t` (`src/gpu.h`) — command execution (nvidia-smi)

Tests inject mock implementations of these vtables instead of real hardware.

### Service Lifecycle

Service starts up by parsing and validating the configuration file.

Then, the service runs with one pthread per sensor type.  Each sensor thread:
- Polls temperature metrics
- Computes suggested fan speed
- Posts the speed to a shared `speed_bus_t` (mutex + condvar)
- Sleeps until next tick using `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)`
  for steady cadence

The main thread waits on the `speed_bus_t` condvar (with a fan-log timeout),
takes the max speed across all sensors, and calls `fan_set_speed()`.

The service gracefully stops on SIGTERM/SIGINT via a `volatile sig_atomic_t
g_shutdown` flag checked in each thread loop.

**Note:** IPMI calls are synchronous and blocking. SIGTERM response time may
be up to one IPMI timeout period (~5–10 seconds) while an IPMI call completes.

### Tests

Tests are required to confirm requirements and prevent regressions during code
maintenance.

Use `libcmocka` for test assertions (`assert_int_equal`, `assert_string_equal`,
`assert_float_equal`, etc.).  Each test file has its own `main()` function and
is compiled and run independently by `make test`.

Shared mock helpers live in `tests/test_helpers.h` and `tests/test_helpers.c`.

Test coverage required for:
- Configuration parsing (YAML, defaults, validation)
- Computing aggregate temperature metrics (rolling buffer)
- Computing suggested fan speeds from mock temperature samples
- Reading CPU temperature metrics via mock IPMI vtable
- Reading GPU temperature metrics via mock command runner
- Fan IPMI control (ReadFanSpeeds, SetSpeed) via mock IPMI vtable

### Error Handling

#### Startup

On startup, validate configuration.  If configuration file cannot be parsed or
contains validation errors, log a descriptive error message and exit.

#### Runtime

If there are errors reading temperature metrics, log a descriptive error and
retry on next iteration.

Log all sensor temperature metrics, computed aggregate metrics, and suggested
fan speed.

If there are errors setting fan speed, log a descriptive error.  Retry on the
next iteration when fan speed is suggested again.

## Launch From Source

The repository provides a `make run` target that:
- Builds the service binary
- Launches it using the configuration file `dev.yaml`
- Log output goes to stdout (since `dev.yaml` has no `log_file` set)

## Software Package Installation

The repository provides a `make package` target that:
- Builds the executable
- Builds a Debian .deb package installable with `apt install` on
  Debian and Ubuntu Linux OS

The package includes the compiled executable, default configuration for
`/etc/frostd.yaml`, and systemd service configuration.

The package installation script steps:
- Copy executable to `/usr/local/bin`
- Copy default config file to `/etc/frostd.yaml`
- Add systemd configuration for `frostd` service
- Start `frostd` service

## Code Style

### Makefile

Use `.PHONY` directives to indicate a target is a descriptive script name and
not a literal filename.  Place the directive on the line before its definition,
not as a single directive for all targets.  This helps prevent forgetting to
update the list.

### C

Write code targeting C23 (`-std=c2x` flag for GCC 13).  Follow standard C best
practices: const-correctness, error-checked return codes, no implicit fallthrough.

- Compile flags: `-std=c2x -D_GNU_SOURCE -Wall -Wextra -Wpedantic`
- No global mutable state except `volatile sig_atomic_t g_shutdown` in main.c
- Memory ownership: functions that allocate with `malloc` expose a matching
  `*_free()` function; the caller is responsible for calling it.
- Error reporting: functions return -1 (or NULL) on failure and write a
  human-readable message into a caller-supplied `char *errbuf, int errbuflen`
  parameter pair.

## License

License: Apache 2.0
