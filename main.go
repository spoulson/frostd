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

	runner := RealRunner{}
	fanCtrl := &IPMIFanController{runner: runner}

	var monitors []*DeviceMonitor
	var interval time.Duration

	if cfg.CPU != nil {
		m := newDeviceMonitor("cpu", cfg.CPU, &CPUReader{runner: runner})
		monitors = append(monitors, m)
		if interval == 0 || cfg.CPU.SampleInterval < interval {
			interval = cfg.CPU.SampleInterval
		}
	}
	if cfg.GPU != nil {
		m := newDeviceMonitor("gpu", cfg.GPU, &GPUReader{runner: runner})
		monitors = append(monitors, m)
		if interval == 0 || cfg.GPU.SampleInterval < interval {
			interval = cfg.GPU.SampleInterval
		}
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()

	logger.Info("frostd started", "config", *configPath, "interval", interval)

	nextTick := time.Now()
	for {
		nextTick = nextTick.Add(interval)

		maxSpeed := 0
		for _, m := range monitors {
			agg, err := m.Poll()
			if err != nil {
				logger.Error("temperature poll failed", "device", m.name, "error", err)
				continue
			}
			logger.Info("temperature", "device", m.name, "aggregate_celsius", fmt.Sprintf("%.1f", agg))

			speed := SuggestSpeed(agg, m.cfg.IdealTemp, m.cfg.MaxTemp)
			logger.Info("suggested fan speed", "device", m.name, "percent", speed)
			if speed > maxSpeed {
				maxSpeed = speed
			}
		}

		logger.Info("setting fan speed", "percent", maxSpeed)
		if err := fanCtrl.SetSpeed(maxSpeed); err != nil {
			logger.Error("failed to set fan speed", "error", err)
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
