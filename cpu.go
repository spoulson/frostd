package main

import (
	"context"
	"fmt"

	ipmi "github.com/bougou/go-ipmi"
)

type CPUReader struct {
	newClient func() (ipmiClient, error)
}

// ReadTemperatures returns CPU package temperatures via IPMI.
// It reads temperature sensors with entity ID 0x03 (processor).
func (r *CPUReader) ReadTemperatures() ([]float64, error) {
	ctx := context.Background()
	client, err := r.newClient()
	if err != nil {
		return nil, fmt.Errorf("creating IPMI client: %w", err)
	}
	if err := client.Connect(ctx); err != nil {
		return nil, fmt.Errorf("connecting to IPMI: %w", err)
	}
	defer client.Close(ctx)

	sensors, err := client.GetSensors(ctx,
		ipmi.SensorFilterOptionIsSensorType(ipmi.SensorTypeTemperature))
	if err != nil {
		return nil, fmt.Errorf("getting IPMI temperature sensors: %w", err)
	}

	var temps []float64
	for _, s := range sensors {
		if uint8(s.EntityID) != 0x03 {
			continue
		}
		temps = append(temps, s.Value)
	}
	if len(temps) == 0 {
		return nil, fmt.Errorf("no CPU temperature sensors found")
	}
	return temps, nil
}
