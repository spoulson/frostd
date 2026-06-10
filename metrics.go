package main

import "fmt"

type TempReader interface {
	ReadTemperatures() (map[string]float64, error)
}

type SensorMonitor struct {
	name    string
	cfg     *SensorConfig
	reader  TempReader
	samples map[string][]float64
}

func newSensorMonitor(name string, cfg *SensorConfig, reader TempReader) *SensorMonitor {
	return &SensorMonitor{
		name:    name,
		cfg:     cfg,
		reader:  reader,
		samples: map[string][]float64{},
	}
}

// Poll reads current temperatures and updates per-sensor-ID rolling sample buffers.
// Returns the latest readings keyed by sensor ID, the rolling aggregate per sensor
// ID, and any error.
func (m *SensorMonitor) Poll() (map[string]float64, map[string]float64, error) {
	temps, err := m.reader.ReadTemperatures()
	if err != nil {
		return nil, nil, fmt.Errorf("%s: reading temperatures: %w", m.name, err)
	}
	if len(temps) == 0 {
		return nil, nil, fmt.Errorf("%s: no temperature readings returned", m.name)
	}

	for id, t := range temps {
		m.samples[id] = append(m.samples[id], t)
		if len(m.samples[id]) > m.cfg.SampleSize {
			m.samples[id] = m.samples[id][len(m.samples[id])-m.cfg.SampleSize:]
		}
	}

	return temps, m.Aggregates(), nil
}

// Aggregates returns the rolling average for each sensor ID.
func (m *SensorMonitor) Aggregates() map[string]float64 {
	result := make(map[string]float64, len(m.samples))
	for id, samples := range m.samples {
		var sum float64
		for _, s := range samples {
			sum += s
		}
		result[id] = sum / float64(len(samples))
	}
	return result
}
