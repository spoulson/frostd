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

type speedUpdate struct {
	sensor string
	speed  int
}

func runSensor(ctx context.Context, m *SensorMonitor, logger *slog.Logger, ch chan<- speedUpdate) {
	nextTick := time.Now()
	for {
		nextTick = nextTick.Add(m.cfg.SampleInterval)

		agg, err := m.Poll()
		if err != nil {
			logger.Error("sensor temperature poll failed", "sensor", m.name, "error", err)
		} else {
			logger.Info("sensor temperature", "sensor", m.name, "aggregate_temp", fmt.Sprintf("%.1f", agg))
			speed := SuggestSpeed(agg, m.cfg.IdealTemp, m.cfg.MaxTemp)
			logger.Info("sensor suggested fan speed", "sensor", m.name, "suggest_percent", speed)
			select {
			case ch <- speedUpdate{sensor: m.name, speed: speed}:
			case <-ctx.Done():
				return
			}
		}

		delay := time.Until(nextTick)
		if delay < 0 {
			delay = 0
		}
		select {
		case <-ctx.Done():
			return
		case <-time.After(delay):
		}
	}
}

func logFanSpeeds(ctx context.Context, fanCtrl FanController, logger *slog.Logger, prev map[string]FanReading) {
	readings, err := fanCtrl.ReadFanSpeeds(ctx)
	if err != nil {
		logger.Error("failed to read current fan speeds", "error", err)
		return
	}
	for _, r := range readings {
		args := []any{"fan", r.Name}
		if r.RPM != nil {
			args = append(args, "rpm", int(*r.RPM))
			if p, ok := prev[r.Name]; ok && p.RPM != nil {
				args = append(args, "delta", int(*r.RPM-*p.RPM))
			}
		}
		if r.Percent != nil {
			args = append(args, "percent", int(*r.Percent))
			if p, ok := prev[r.Name]; ok && p.Percent != nil {
				args = append(args, "delta", int(*r.Percent-*p.Percent))
			}
		}
		logger.Info("current fan speed", args...)
		prev[r.Name] = r
	}
}

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

	if cfg.CPU != nil {
		monitors = append(monitors, newSensorMonitor("cpu", cfg.CPU, &CPUReader{newClient: newRealIPMIClient}))
	}
	if cfg.GPU != nil {
		monitors = append(monitors, newSensorMonitor("gpu", cfg.GPU, &GPUReader{runner: RealRunner{}}))
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()

	logger.Info("frostd started", "config", *configPath, "dry_run", cfg.DryRun)

	prevFanReadings := map[string]FanReading{}

	// Initial fan speed log.
	logFanSpeeds(ctx, fanCtrl, logger, prevFanReadings)

	ch := make(chan speedUpdate, len(monitors))
	latestSpeeds := make(map[string]int, len(monitors))
	for _, m := range monitors {
		latestSpeeds[m.name] = 0
		go runSensor(ctx, m, logger, ch)
	}

	fanLogTicker := time.NewTicker(cfg.FanLogInterval)
	defer fanLogTicker.Stop()

	pendingUpdate := false

	for {
		select {
		case <-ctx.Done():
			logger.Info("frostd stopping")
			return
		case <-fanLogTicker.C:
			logFanSpeeds(ctx, fanCtrl, logger, prevFanReadings)
		case update := <-ch:
			latestSpeeds[update.sensor] = update.speed
			pendingUpdate = true
		}

		if !pendingUpdate {
			continue
		}
		pendingUpdate = false

		maxSpeed := 0
		for _, s := range latestSpeeds {
			if s > maxSpeed {
				maxSpeed = s
			}
		}

		if cfg.DryRun {
			logger.Info("dry run: skipping fan speed change", "system_percent", maxSpeed)
		} else {
			logger.Info("setting fan speed", "system_percent", maxSpeed)
			if err := fanCtrl.SetSpeed(ctx, maxSpeed); err != nil {
				logger.Error("failed to set fan speed", "error", err)
			}
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
