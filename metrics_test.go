package main

import (
	"errors"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

type staticReader struct {
	temps map[string]float64
	err   error
}

func (r *staticReader) ReadTemperatures() (map[string]float64, error) {
	return r.temps, r.err
}

func testSensorConfig() *SensorConfig {
	return &SensorConfig{
		IdealTemp:      40,
		MaxTemp:        75,
		SampleSize:     3,
		SampleInterval: 15 * time.Second,
	}
}

func TestSensorMonitor_AccumulatesSamples(t *testing.T) {
	reader := &staticReader{temps: map[string]float64{"s0": 50, "s1": 60}}
	m := newSensorMonitor("cpu", testSensorConfig(), reader)
	temps, aggs, err := m.Poll()
	require.NoError(t, err)
	assert.Equal(t, map[string]float64{"s0": 50, "s1": 60}, temps)
	assert.Len(t, m.samples, 2)
	assert.Equal(t, map[string]float64{"s0": 50.0, "s1": 60.0}, aggs)
}

func TestSensorMonitor_CapsSamplesAtSampleSize(t *testing.T) {
	cfg := testSensorConfig()
	cfg.SampleSize = 3
	reader := &staticReader{temps: map[string]float64{"s0": 0}}
	m := newSensorMonitor("cpu", cfg, reader)

	for i := 0; i < 5; i++ {
		reader.temps = map[string]float64{"s0": float64(i * 10)}
		_, _, err := m.Poll()
		require.NoError(t, err)
	}
	assert.Len(t, m.samples["s0"], 3)
	// last 3 polls: 20, 30, 40
	assert.Equal(t, (20.0+30.0+40.0)/3, m.Aggregates()["s0"])
}

func TestSensorMonitor_AggregatesBeforeAnyPolls(t *testing.T) {
	m := newSensorMonitor("cpu", testSensorConfig(), &staticReader{temps: map[string]float64{"s0": 50}})
	assert.Empty(t, m.Aggregates())
}

func TestSensorMonitor_ReaderError(t *testing.T) {
	reader := &staticReader{err: errors.New("ipmitool failed")}
	m := newSensorMonitor("cpu", testSensorConfig(), reader)
	_, _, err := m.Poll()
	assert.Error(t, err)
}

func TestSensorMonitor_EmptyReadings(t *testing.T) {
	reader := &staticReader{temps: map[string]float64{}}
	m := newSensorMonitor("cpu", testSensorConfig(), reader)
	_, _, err := m.Poll()
	assert.Error(t, err)
}

func TestSensorMonitor_MultipleSensorIDsHaveIndependentAggregates(t *testing.T) {
	cfg := testSensorConfig()
	cfg.SampleSize = 2
	reader := &staticReader{temps: map[string]float64{"s0": 50, "s1": 70}}
	m := newSensorMonitor("cpu", cfg, reader)

	_, _, err := m.Poll()
	require.NoError(t, err)
	reader.temps = map[string]float64{"s0": 60, "s1": 80}
	_, aggs, err := m.Poll()
	require.NoError(t, err)

	assert.Equal(t, (50.0+60.0)/2, aggs["s0"])
	assert.Equal(t, (70.0+80.0)/2, aggs["s1"])
}
