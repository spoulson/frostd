package main

import (
	"errors"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

type staticReader struct {
	temps []float64
	err   error
}

func (r *staticReader) ReadTemperatures() ([]float64, error) {
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
	reader := &staticReader{temps: []float64{50, 60}}
	m := newSensorMonitor("cpu", testSensorConfig(), reader)
	agg, err := m.Poll()
	require.NoError(t, err)
	assert.Len(t, m.samples, 1)
	assert.Equal(t, 55.0, agg) // avg of [50,60]
}

func TestSensorMonitor_CapsSamplesAtSampleSize(t *testing.T) {
	cfg := testSensorConfig()
	cfg.SampleSize = 3
	reader := &staticReader{temps: []float64{0}}
	m := newSensorMonitor("cpu", cfg, reader)

	for i := 0; i < 5; i++ {
		reader.temps = []float64{float64(i * 10)}
		_, err := m.Poll()
		require.NoError(t, err)
	}
	assert.Len(t, m.samples, 3)
	// last 3 polls: 20, 30, 40
	assert.Equal(t, (20.0+30.0+40.0)/3, m.Aggregate())
}

func TestSensorMonitor_AggregateBeforeAnyPolls(t *testing.T) {
	m := newSensorMonitor("cpu", testSensorConfig(), &staticReader{temps: []float64{50}})
	assert.Equal(t, 0.0, m.Aggregate())
}

func TestSensorMonitor_ReaderError(t *testing.T) {
	reader := &staticReader{err: errors.New("ipmitool failed")}
	m := newSensorMonitor("cpu", testSensorConfig(), reader)
	_, err := m.Poll()
	assert.Error(t, err)
}

func TestSensorMonitor_EmptyReadings(t *testing.T) {
	reader := &staticReader{temps: []float64{}}
	m := newSensorMonitor("cpu", testSensorConfig(), reader)
	_, err := m.Poll()
	assert.Error(t, err)
}

func TestSensorMonitor_MultipleSensorIDsAveraged(t *testing.T) {
	reader := &staticReader{temps: []float64{50, 70}}
	m := newSensorMonitor("cpu", testSensorConfig(), reader)
	agg, err := m.Poll()
	require.NoError(t, err)
	assert.Equal(t, 60.0, agg)
}
