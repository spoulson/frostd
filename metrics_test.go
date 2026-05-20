package main

import (
	"errors"
	"testing"
	"time"
)

type staticReader struct {
	temps []float64
	err   error
}

func (r *staticReader) ReadTemperatures() ([]float64, error) {
	return r.temps, r.err
}

func testDeviceConfig() *DeviceConfig {
	return &DeviceConfig{
		IdealTemp:      40,
		MaxTemp:        75,
		SampleSize:     3,
		SampleInterval: 15 * time.Second,
	}
}

func TestDeviceMonitor_AccumulatesSamples(t *testing.T) {
	reader := &staticReader{temps: []float64{50, 60}}
	m := newDeviceMonitor("cpu", testDeviceConfig(), reader)

	agg, err := m.Poll()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(m.samples) != 1 {
		t.Errorf("samples len = %d, want 1", len(m.samples))
	}
	// avg of [50,60] = 55
	if agg != 55 {
		t.Errorf("aggregate = %.1f, want 55", agg)
	}
}

func TestDeviceMonitor_CapsSamplesAtSampleSize(t *testing.T) {
	cfg := testDeviceConfig()
	cfg.SampleSize = 3
	reader := &staticReader{temps: []float64{50}}
	m := newDeviceMonitor("cpu", cfg, reader)

	for i := 0; i < 5; i++ {
		reader.temps = []float64{float64(i * 10)}
		if _, err := m.Poll(); err != nil {
			t.Fatalf("Poll error: %v", err)
		}
	}
	if len(m.samples) != 3 {
		t.Errorf("samples len = %d, want 3", len(m.samples))
	}
	// last 3 polls: temps 20, 30, 40 → all are single-value averages
	want := (20.0 + 30.0 + 40.0) / 3
	if m.Aggregate() != want {
		t.Errorf("Aggregate = %.2f, want %.2f", m.Aggregate(), want)
	}
}

func TestDeviceMonitor_AggregateBeforeAnyPolls(t *testing.T) {
	m := newDeviceMonitor("cpu", testDeviceConfig(), &staticReader{temps: []float64{50}})
	if m.Aggregate() != 0 {
		t.Errorf("Aggregate before polls = %.1f, want 0", m.Aggregate())
	}
}

func TestDeviceMonitor_ReaderError(t *testing.T) {
	reader := &staticReader{err: errors.New("ipmitool failed")}
	m := newDeviceMonitor("cpu", testDeviceConfig(), reader)
	_, err := m.Poll()
	if err == nil {
		t.Fatal("expected error from failing reader")
	}
}

func TestDeviceMonitor_EmptyReadings(t *testing.T) {
	reader := &staticReader{temps: []float64{}}
	m := newDeviceMonitor("cpu", testDeviceConfig(), reader)
	_, err := m.Poll()
	if err == nil {
		t.Fatal("expected error for empty temperature readings")
	}
}

func TestDeviceMonitor_MultipleDeviceIDsAveraged(t *testing.T) {
	// Two CPU packages at 50 and 70 → avg = 60 stored as one sample
	reader := &staticReader{temps: []float64{50, 70}}
	m := newDeviceMonitor("cpu", testDeviceConfig(), reader)
	agg, err := m.Poll()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if agg != 60 {
		t.Errorf("aggregate = %.1f, want 60", agg)
	}
}
