package main

import (
	"context"
	"log/slog"
	"net/http"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

type PrometheusMetrics struct {
	deviceTemp      *prometheus.GaugeVec
	aggregateTemp   *prometheus.GaugeVec
	suggestedSpeed  *prometheus.GaugeVec
	fanSpeedRPM     *prometheus.GaugeVec
	fanSpeedPercent *prometheus.GaugeVec
	systemSpeed     prometheus.Gauge
	registry        *prometheus.Registry
}

func newPrometheusMetrics() *PrometheusMetrics {
	reg := prometheus.NewRegistry()
	m := &PrometheusMetrics{
		deviceTemp: prometheus.NewGaugeVec(prometheus.GaugeOpts{
			Name: "frostd_device_temperature",
			Help: "Current temperature reading per device sensor, in Celsius.",
		}, []string{"sensor", "id"}),
		aggregateTemp: prometheus.NewGaugeVec(prometheus.GaugeOpts{
			Name: "frostd_device_aggregate_temperature",
			Help: "Rolling aggregate temperature per sensor type, in Celsius.",
		}, []string{"sensor"}),
		suggestedSpeed: prometheus.NewGaugeVec(prometheus.GaugeOpts{
			Name: "frostd_suggested_fan_speed_percent",
			Help: "Suggested fan speed percentage per sensor type.",
		}, []string{"sensor"}),
		fanSpeedRPM: prometheus.NewGaugeVec(prometheus.GaugeOpts{
			Name: "frostd_actual_fan_rpm",
			Help: "Actual fan speed in RPM.",
		}, []string{"fan"}),
		fanSpeedPercent: prometheus.NewGaugeVec(prometheus.GaugeOpts{
			Name: "frostd_actual_fan_speed_percent",
			Help: "Actual fan speed as percentage.",
		}, []string{"fan"}),
		systemSpeed: prometheus.NewGauge(prometheus.GaugeOpts{
			Name: "frostd_system_fan_speed_percent",
			Help: "Commanded system fan speed percentage.",
		}),
		registry: reg,
	}
	reg.MustRegister(
		m.deviceTemp,
		m.aggregateTemp,
		m.suggestedSpeed,
		m.fanSpeedRPM,
		m.fanSpeedPercent,
		m.systemSpeed,
	)
	return m
}

func startPrometheusServer(ctx context.Context, addr string, m *PrometheusMetrics, logger *slog.Logger) {
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.HandlerFor(m.registry, promhttp.HandlerOpts{}))
	srv := &http.Server{Addr: addr, Handler: mux}

	go func() {
		<-ctx.Done()
		if err := srv.Shutdown(context.Background()); err != nil {
			logger.Error("prometheus server shutdown error", "error", err)
		}
	}()

	go func() {
		logger.Info("prometheus metrics server listening", "addr", addr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logger.Error("prometheus metrics server error", "error", err)
		}
	}()
}
