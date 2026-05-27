package main

import (
	"fmt"
	"strconv"
	"strings"
)

type GPUReader struct {
	runner CommandRunner
}

// ReadTemperatures returns GPU temperatures via nvidia-smi, keyed by "gpu0", "gpu1", etc.
func (r *GPUReader) ReadTemperatures() (map[string]float64, error) {
	out, err := r.runner.Run("nvidia-smi", "--query-gpu=temperature.gpu", "--format=csv,noheader")
	if err != nil {
		return nil, fmt.Errorf("nvidia-smi: %w", err)
	}
	return parseGPUTemps(string(out))
}

// parseGPUTemps parses nvidia-smi CSV output (one temperature per line).
func parseGPUTemps(output string) (map[string]float64, error) {
	temps := map[string]float64{}
	for _, line := range strings.Split(strings.TrimSpace(output), "\n") {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		temp, err := strconv.ParseFloat(line, 64)
		if err != nil {
			return nil, fmt.Errorf("parsing GPU temperature %q: %w", line, err)
		}
		temps[fmt.Sprintf("gpu%d", len(temps))] = temp
	}
	if len(temps) == 0 {
		return nil, fmt.Errorf("no GPU temperatures found in nvidia-smi output")
	}
	return temps, nil
}
