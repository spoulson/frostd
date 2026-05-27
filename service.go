package main

import (
	"context"
	"fmt"
	"log/slog"
	"time"
)

type speedUpdate struct {
	sensor string
	speed  int
}

func run(ctx context.Context, cfg *Config, fanCtrl FanController, monitors []*SensorMonitor, logger *slog.Logger) {
	prevFanReadings := map[string]FanReading{}
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
	lastSpeed := -1

	for {
		select {
		case <-ctx.Done():
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
			s = max(s, maxSpeed)
		}

		if cfg.DryRun {
			logger.Info("dry run: skipping fan speed change", "system_percent", maxSpeed)
			continue
		}
		if maxSpeed == lastSpeed {
			continue
		}

		lastSpeed = maxSpeed
		logger.Info("setting fan speed", "system_percent", maxSpeed)
		if err := fanCtrl.SetSpeed(ctx, maxSpeed); err != nil {
			logger.Error("failed to set fan speed", "error", err)
		}
	}
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
