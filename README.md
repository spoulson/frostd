# frostd

`frostd` is a daemon that dynamically regulates chassis fan speeds on Dell
PowerEdge 12th generation servers (e.g. R720) based on real-time temperature
metrics. It reads CPU temperatures via IPMI and optionally GPU temperatures via
`nvidia-smi`, then applies a parabolic easing curve to derive a fan speed
percentage that keeps hardware cool without running fans harder than necessary.

## Why not use automatic fans?

The BMC can dynamically adjust fan speed to keep CPU temps cool, but it doesn't
know how to regulate GPU temps.

Additionally, the BMC settings are non-adjustable.  With `frostd` it is
possible to set much lower fan speeds than usual and still keep all components
cool.  This allows for a measurable power reduction at idle.

## Features

- Monitors CPU package temperatures via IPMI (no `ipmitool` dependency)
- Optional NVIDIA GPU temperature monitoring via `nvidia-smi`
- Parabolic fan speed curve between configurable ideal and maximum temperatures
- Independent polling intervals per sensor type
- Rolling sample window to smooth transient temperature spikes
- Dry-run mode for observing behaviour without changing fan speeds
- Structured logging (text or JSON) to file or stdout
- Runs as a systemd service

## Requirements

- Dell PowerEdge 12th generation server (e.g. R720)
- Debian or Ubuntu Linux, x86-64
- Go 1.26+ (build only)
- `make` (build only)
- Compatible NVIDIA driver and CUDA toolkit (GPU monitoring only)

## Dev Tooling
### Build

```sh
make build
```

The compiled binary is written to `./frostd`.

### Run from source

```sh
make run
```

Launches the service using `dev.yaml` as the configuration file, with log
output to stdout.

### Local install

```sh
make install
```

This will:

1. Build the binary
2. Copy `frostd.yaml` to `/etc/frostd.yaml` (skipped if the file already exists)
3. Install the logrotate configuration to `/etc/logrotate.d/frostd`
4. Install and enable the `frostd` systemd service

### Local uninstall

```sh
make uninstall
```

This reverts an install from `make install`.  Stops and removes the systemd
service. The configuration file and logs are preserved.

```sh
make uninstall_clean
```

Same as `uninstall`, but also removes `/etc/frostd.yaml` and the log file
specified within it.

## Package install
Build and install a Debian .deb package.

```sh
make package
```

Produces a `frostd_<version>_amd64.deb` file that can be installed with:

```sh
sudo apt install ./frostd_<version>_amd64.deb
```

## Configuration

The default configuration file path is `/etc/frostd.yaml`. Override with:

```sh
frostd -c /path/to/config.yaml
```

### Top-level options

| Field              | Type     | Default                        | Description |
|--------------------|----------|--------------------------------|-------------|
| `log_file`         | string   | *(stdout)*                     | Path to log file. Leave empty to log to stdout. |
| `log_format`       | string   | `text`                         | Log format: `text` or `json`. |
| `dry_run`          | bool     | `false`                        | Log suggested fan speeds without applying them. |
| `fan_log_interval` | duration | `15s`                          | How often to log current fan speeds. |

### Sensor options (`cpu` / `gpu`)

Both `cpu` and `gpu` sections are optional, but at least one must be enabled.

| Field             | Type     | Default | Description |
|-------------------|----------|---------|-------------|
| `ideal_temp`      | float    | `40`    | Temperature (°C) at which fans run at minimum speed. |
| `max_temp`        | float    | `75`    | Temperature (°C) at which fans run at full speed. |
| `sample_size`     | int      | `3`     | Number of polling samples to retain in the rolling window. |
| `sample_interval` | duration | `15s`   | How often to poll temperature for this sensor type. |

### Fan speed algorithm

For each sensor type, `frostd` polls temperature, takes the maximum reading
across all sensor IDs, and maintains a rolling average of the last
`sample_size` maximums. The suggested fan speed is:

```
suggested = min(100, (actual - ideal)² × 100 / (max - ideal)²)
```

The system fan speed is set to the highest suggestion across all active sensor
types. Fan speed is only updated when at least one sensor has reported since
the last update, and only when the suggested speed has changed.

### Example configuration

```yaml
log_file: /var/log/frostd/frostd.log
log_format: json
dry_run: false
fan_log_interval: 15s

cpu:
  ideal_temp: 40
  max_temp: 75
  sample_size: 3
  sample_interval: 15s

gpu:
  ideal_temp: 45
  max_temp: 80
  sample_size: 3
  sample_interval: 10s
```

## License

Apache 2.0 — see [LICENSE](LICENSE).
