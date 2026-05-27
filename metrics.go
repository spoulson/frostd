package main

import "fmt"

type TempReader interface {
	ReadTemperatures() (map[string]float64, error)
}

type SensorMonitor struct {
	name    string
	cfg     *SensorConfig
	reader  TempReader
	samples []float64
}

func newSensorMonitor(name string, cfg *SensorConfig, reader TempReader) *SensorMonitor {
	return &SensorMonitor{
		name:   name,
		cfg:    cfg,
		reader: reader,
	}
}

// Poll reads current temperatures and updates the rolling sample buffer.
// Returns the latest readings keyed by sensor ID, the aggregate (average of
// per-poll maximums) across samples, and any error.
func (m *SensorMonitor) Poll() (map[string]float64, float64, error) {
	temps, err := m.reader.ReadTemperatures()
	if err != nil {
		return nil, 0, fmt.Errorf("%s: reading temperatures: %w", m.name, err)
	}
	if len(temps) == 0 {
		return nil, 0, fmt.Errorf("%s: no temperature readings returned", m.name)
	}

	var maxTemp float64
	for _, t := range temps {
		if t > maxTemp {
			maxTemp = t
		}
	}

	m.samples = append(m.samples, maxTemp)
	if len(m.samples) > m.cfg.SampleSize {
		m.samples = m.samples[len(m.samples)-m.cfg.SampleSize:]
	}

	return temps, m.Aggregate(), nil
}

// Aggregate returns the average of collected samples.
func (m *SensorMonitor) Aggregate() float64 {
	if len(m.samples) == 0 {
		return 0
	}
	var sum float64
	for _, s := range m.samples {
		sum += s
	}
	return sum / float64(len(m.samples))
}
