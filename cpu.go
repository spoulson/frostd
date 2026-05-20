package main

import (
	"fmt"
	"strconv"
	"strings"
)

type CPUReader struct {
	runner CommandRunner
}

// ReadTemperatures returns CPU package temperatures via ipmitool.
// It parses `ipmitool sdr type Temperature` output for processor sensors (entity 3.x).
func (r *CPUReader) ReadTemperatures() ([]float64, error) {
	out, err := r.runner.Run("ipmitool", "sdr", "type", "Temperature")
	if err != nil {
		return nil, fmt.Errorf("ipmitool sdr type Temperature: %w", err)
	}
	return parseCPUTemps(string(out))
}

// parseCPUTemps extracts processor (entity 3.x) temperatures from ipmitool SDR output.
// Example line:
//
//	Temp             | 0Eh | ok  | 3.1 | 45 degrees C
func parseCPUTemps(output string) ([]float64, error) {
	var temps []float64
	for _, line := range strings.Split(output, "\n") {
		fields := strings.Split(line, "|")
		if len(fields) < 5 {
			continue
		}
		entity := strings.TrimSpace(fields[3])
		// Processor entity IDs start with "3."
		if !strings.HasPrefix(entity, "3.") {
			continue
		}
		val := strings.TrimSpace(fields[4])
		val = strings.TrimSuffix(val, " degrees C")
		val = strings.TrimSpace(val)
		temp, err := strconv.ParseFloat(val, 64)
		if err != nil {
			continue
		}
		temps = append(temps, temp)
	}
	if len(temps) == 0 {
		return nil, fmt.Errorf("no CPU temperature sensors found in ipmitool output")
	}
	return temps, nil
}
