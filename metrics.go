package main

import "fmt"

type TempReader interface {
	ReadTemperatures() ([]float64, error)
}

type DeviceMonitor struct {
	name    string
	cfg     *DeviceConfig
	reader  TempReader
	samples []float64
}

func newDeviceMonitor(name string, cfg *DeviceConfig, reader TempReader) *DeviceMonitor {
	return &DeviceMonitor{
		name:   name,
		cfg:    cfg,
		reader: reader,
	}
}

// Poll reads current temperatures and updates the rolling sample buffer.
// Returns the aggregate (average) temperature across all device IDs and samples.
func (m *DeviceMonitor) Poll() (float64, error) {
	temps, err := m.reader.ReadTemperatures()
	if err != nil {
		return 0, fmt.Errorf("%s: reading temperatures: %w", m.name, err)
	}
	if len(temps) == 0 {
		return 0, fmt.Errorf("%s: no temperature readings returned", m.name)
	}

	var sum float64
	for _, t := range temps {
		sum += t
	}
	avg := sum / float64(len(temps))

	m.samples = append(m.samples, avg)
	if len(m.samples) > m.cfg.SampleSize {
		m.samples = m.samples[len(m.samples)-m.cfg.SampleSize:]
	}

	return m.Aggregate(), nil
}

// Aggregate returns the average of collected samples.
func (m *DeviceMonitor) Aggregate() float64 {
	if len(m.samples) == 0 {
		return 0
	}
	var sum float64
	for _, s := range m.samples {
		sum += s
	}
	return sum / float64(len(m.samples))
}
