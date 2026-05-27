package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func main() {
	configPath := flag.String("c", "/etc/frostd.yaml", "path to config file")
	flag.Parse()

	cfg, err := loadConfig(*configPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "frostd: configuration error: %v\n", err)
		os.Exit(1)
	}

	logger, closer, err := setupLogger(cfg.LogFile)
	if err != nil {
		fmt.Fprintf(os.Stderr, "frostd: failed to open log file: %v\n", err)
		os.Exit(1)
	}
	defer closer()

	fanCtrl := &IPMIFanController{newClient: newRealIPMIClient}

	var monitors []*SensorMonitor
	var interval time.Duration

	if cfg.CPU != nil {
		m := newSensorMonitor("cpu", cfg.CPU, &CPUReader{newClient: newRealIPMIClient})
		monitors = append(monitors, m)
		if interval == 0 || cfg.CPU.SampleInterval < interval {
			interval = cfg.CPU.SampleInterval
		}
	}
	if cfg.GPU != nil {
		m := newSensorMonitor("gpu", cfg.GPU, &GPUReader{runner: RealRunner{}})
		monitors = append(monitors, m)
		if interval == 0 || cfg.GPU.SampleInterval < interval {
			interval = cfg.GPU.SampleInterval
		}
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()

	logger.Info("frostd started", "config", *configPath, "interval", interval, "dry_run", cfg.DryRun)

	nextTick := time.Now()
	for {
		nextTick = nextTick.Add(interval)

		maxSpeed := 0
		for _, m := range monitors {
			agg, err := m.Poll()
			if err != nil {
				logger.Error("temperature poll failed", "sensor", m.name, "error", err)
				continue
			}
			logger.Info("temperature", "sensor", m.name, "aggregate_temp", fmt.Sprintf("%.1f", agg))

			speed := SuggestSpeed(agg, m.cfg.IdealTemp, m.cfg.MaxTemp)
			if speed > maxSpeed {
				maxSpeed = speed
			}
			logger.Info("suggested fan speed", "sensor", m.name, "required_percent", speed)
		}

		if readings, err := fanCtrl.ReadFanSpeeds(); err != nil {
			logger.Error("failed to read current fan speeds", "error", err)
		} else {
			for _, r := range readings {
				args := []any{"fan", r.Name}
				if r.RPM != nil {
					args = append(args, "rpm", int(*r.RPM))
				}
				if r.Percent != nil {
					args = append(args, "percent", int(*r.Percent))
				}
				logger.Info("current fan speed", args...)
			}
		}

		if cfg.DryRun {
			logger.Info("dry run: skipping fan speed change", "system_percent", maxSpeed)
		} else {
			logger.Info("setting fan speed", "system_percent", maxSpeed)
			if err := fanCtrl.SetSpeed(maxSpeed); err != nil {
				logger.Error("failed to set fan speed", "error", err)
			}
		}

		delay := time.Until(nextTick)
		if delay < 0 {
			delay = 0
		}
		select {
		case <-ctx.Done():
			logger.Info("frostd stopping")
			return
		case <-time.After(delay):
		}
	}
}

func setupLogger(logFile string) (*slog.Logger, func(), error) {
	if logFile == "" {
		return slog.New(slog.NewTextHandler(os.Stdout, nil)), func() {}, nil
	}
	if err := os.MkdirAll(dirOf(logFile), 0o755); err != nil {
		return nil, nil, err
	}
	f, err := os.OpenFile(logFile, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0o644)
	if err != nil {
		return nil, nil, err
	}
	logger := slog.New(slog.NewTextHandler(f, nil))
	return logger, func() { f.Close() }, nil
}

func dirOf(path string) string {
	for i := len(path) - 1; i >= 0; i-- {
		if path[i] == '/' {
			return path[:i]
		}
	}
	return "."
}
