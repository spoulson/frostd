package main

import (
	"context"
	"log/slog"
	"time"
)

type speedUpdate struct {
	sensor string
	speed  int
}

type Service struct {
	cfg      *Config
	fanCtrl  FanController
	monitors []*SensorMonitor
	logger   *slog.Logger
	prom     *PrometheusMetrics
}

func (s *Service) run(ctx context.Context) {
	prevFanReadings := map[string]FanReading{}
	s.logFanSpeeds(ctx, prevFanReadings)

	speedCh := make(chan speedUpdate, len(s.monitors))
	latestSpeeds := make(map[string]int, len(s.monitors))
	for _, m := range s.monitors {
		latestSpeeds[m.name] = 0
		go s.runSensor(ctx, m, speedCh)
	}

	fanLogTicker := time.NewTicker(s.cfg.FanLogInterval)
	defer fanLogTicker.Stop()
	pendingUpdate := false
	lastSpeed := -1

	for {
		select {
		case <-ctx.Done():
			return
		case <-fanLogTicker.C:
			s.logFanSpeeds(ctx, prevFanReadings)
		case update := <-speedCh:
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
		if maxSpeed == lastSpeed {
			continue
		}
		lastSpeed = maxSpeed

		s.prom.systemSpeed.Set(float64(maxSpeed))

		if s.cfg.DryRun {
			s.logger.Info("dry run: skipping fan speed change", "system_percent", maxSpeed)
			continue
		}

		s.logger.Info("setting fan speed", "system_percent", maxSpeed)
		if err := s.fanCtrl.SetSpeed(ctx, maxSpeed); err != nil {
			s.logger.Error("failed to set fan speed", "error", err)
		}
	}
}

func (s *Service) runSensor(ctx context.Context, m *SensorMonitor, ch chan<- speedUpdate) {
	nextTick := time.Now()
	for {
		nextTick = nextTick.Add(m.cfg.SampleInterval)

		temps, aggs, err := m.Poll()
		if err != nil {
			s.logger.Error("sensor temperature poll failed", "sensor", m.name, "error", err)
		} else {
			s.logger.Info("sensor temperature", "sensor", m.name, "latest_temps", temps, "aggregate_temps", aggs)
			var maxAgg float64
			for _, a := range aggs {
				if a > maxAgg {
					maxAgg = a
				}
			}
			speed := SuggestSpeed(maxAgg, m.cfg.IdealTemp, m.cfg.MaxTemp)
			s.logger.Info("sensor suggested fan speed", "sensor", m.name, "suggest_percent", speed)
			for id, t := range temps {
				s.prom.sensorTemp.WithLabelValues(m.name, id).Set(t)
			}
			for id, a := range aggs {
				s.prom.aggregateTemp.WithLabelValues(m.name, id).Set(a)
			}
			s.prom.suggestedSpeed.WithLabelValues(m.name).Set(float64(speed))
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

func (s *Service) logFanSpeeds(ctx context.Context, prev map[string]FanReading) {
	readings, err := s.fanCtrl.ReadFanSpeeds(ctx)
	if err != nil {
		s.logger.Error("failed to read current fan speeds", "error", err)
		return
	}
	for _, r := range readings {
		args := []any{"fan", r.Name}
		if r.RPM != nil {
			args = append(args, "rpm", int(*r.RPM))
			if p, ok := prev[r.Name]; ok && p.RPM != nil {
				args = append(args, "delta", int(*r.RPM-*p.RPM))
			}
			s.prom.fanSpeedRPM.WithLabelValues(r.Name).Set(*r.RPM)
		}
		if r.Percent != nil {
			s.prom.fanSpeedPercent.WithLabelValues(r.Name).Set(*r.Percent)
		}
		s.logger.Info("current fan speed", args...)
		prev[r.Name] = r
	}
}
