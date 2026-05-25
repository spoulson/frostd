# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project

`frostd` is a daemon that dynamically regulates fan speeds on Dell PowerEdge
servers based on hardware temperature metrics.

`frostd` supports Dell PowerEdge 12th generation servers, such as R720.

`frostd` identifies supported devices and actively monitors metrics and adjusts
chassis fan speed using IPMI.

## Device Metrics

Devices are grouped by device type.  Each device type has zero or more device
IDs.

### CPU Device Metrics

`frostd` uses IPMI to read CPU temperatures of each package.  Each package is
listed as a device ID.

### NVIDIA Tesla P40 GPU Device Metrics

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

For each device type, read the temperature and compute aggregate temperature
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
- `actual_temp` is the device aggregate temperature metric.
- Set fan speed to the max `suggested_speed` computed from all devices. This
ensures adequate airflow for the device that needs it most.

## Configuration

frostd reads a YAML config file in the default path `/etc/frostd.yaml` or
overridden by specifying CLI argument `-c <file>`.

The YAML config specifies which device types to monitor: CPU and/or NVIDIA
Tesla P40 GPU.  Each device type includes settings for:
- ideal temperature in Celcius (Default 40)
- maximum temperature in Celcius (Default 75)
- sample size (Default 3)
- sample time interval (Default 15s)

All instances of a device type will be discovered (e.g. multiple CPU packages,
multiple GPUs).

Log file output file is specified (Default `/var/log/frostd/frostd.log`).

Dry run mode is enabled with `dry_run: true` (Default `false`).  When enabled,
frostd monitors temperature metrics and logs suggested fan speeds but does not
issue any fan speed changes via IPMI.  Useful for observing behavior without
affecting hardware.

## Technology Stack

`frostd` is written in Golang and compiled to standalone executable file to be
run on Debian or Ubuntu Linux on x86-64 architecture.  The executable runs as a
systemd service.

## Developer Prerequisites

- Debian or Ubuntu Linux OS on x86-64 architecture
- Golang 1.26+ installed
- `make` installed

## Software Design

### Technology Stack

- `frostd` is a systemd service written in latest Golang.
- It must be run on a Dell PowerEdge 12th generation server.

### Tooling

- Use `slog` package for logging.
- Use module `github.com/bougou/go-ipmi` for IPMI support.  Do not depend on
the commonly used `ipmitool` command.

### Service Lifecycle

Service starts up by parsing and validating the configuration file.

Then, the service runs in a loop.  On each iteration:
- Polls temperature metrics
- Compute suggested fan speed
- Set chassis fan speed
- Sleep until next interval
  - Sleep to incremental times, not static durations.  This ensures loops occur
  on steady cadence even if one iteration takes a bit longer to process.

The service immediately and gracefully stops when requested with command:
`systemctl stop frostd`.

### Tests

Tests are required to confirm requirements and prevent regressions during code
maintenance.

Use module `github.com/stretchr/testify` for `assert` and `require` packages
for simplifying assertions and validation logic.

End-to-end tests are required for functionality:
- Configuration parsing
    - YAML file structure
    - Validation for each field with happy path, numeric range errors, and likely special case checks
- Computing aggregate temperature metrics
- Computing suggested fan speeds from mock temperature samples
- Reading actual CPU temperature metrics
- Reading actual NVIDIA GPU temperature metrics using `nvidia-smi`

### Error Handling

#### Startup

On startup, validate configuration.  If configuration file cannot be parsed or
contains validation errors, log a descriptive error message and exit.

#### Runtime

If there are errors reading temperature metrics, log a descriptive error and
retry on next iteration.

Log all device temperature metrics, computed aggregate metrics, and suggested
fan speed.

If there are errors setting fan speed, log a descriptive error.  Retry on the
next iteration when fan speed is suggested again.

## Launch From Source

The repository will provide a `make run` target that will:
- Call `go run` to launch the service using the configuration file `dev.yaml`.
- `dev.yaml` is a copy of the default configuration file used for installing
`/etc/frostd.yaml`.
- Log output goes to stdout instead of a log file.

## Software Package Installation

The repository will provide a `make package` target that will:
- Build the executable
- Build a Debian .deb package that can be installed with `apt install` on
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

### Golang

Write code for latest Golang version. Follow Golang's current standards of code
style and best practices as outlined in Effective Go by Google.

## License

License: Apache 2.0
