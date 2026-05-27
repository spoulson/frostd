package main

import (
	"context"
	"fmt"
	"math"

	ipmi "github.com/bougou/go-ipmi"
)

// SuggestSpeed computes fan speed [0,100] using a parabolic easing curve.
func SuggestSpeed(actualTemp, idealTemp, maxTemp float64) int {
	if actualTemp <= idealTemp {
		return 0
	}
	speed := math.Pow(actualTemp-idealTemp, 2) * (100 / math.Pow(maxTemp-idealTemp, 2))
	if speed > 100 {
		return 100
	}
	return int(math.Round(speed))
}

type FanReading struct {
	Name    string
	RPM     *float64
	Percent *float64
}

type FanController interface {
	ReadFanSpeeds() ([]FanReading, error)
	SetSpeed(percent int) error
}

type IPMIFanController struct {
	newClient func() (ipmiClient, error)
}

// ReadFanSpeeds returns current readings from all IPMI fan sensors.
// Each reading includes RPM if the sensor reports in RPM, or percent if the sensor reports as a percentage.
func (c *IPMIFanController) ReadFanSpeeds() ([]FanReading, error) {
	ctx := context.Background()
	client, err := c.newClient()
	if err != nil {
		return nil, fmt.Errorf("creating IPMI client: %w", err)
	}
	if err := client.Connect(ctx); err != nil {
		return nil, fmt.Errorf("connecting to IPMI: %w", err)
	}
	defer client.Close(ctx)

	sensors, err := client.GetSensors(ctx,
		ipmi.SensorFilterOptionIsSensorType(ipmi.SensorTypeFan))
	if err != nil {
		return nil, fmt.Errorf("getting IPMI fan sensors: %w", err)
	}

	var readings []FanReading
	for _, s := range sensors {
		if !s.HasAnalogReading {
			continue
		}
		r := FanReading{Name: s.Name}
		if s.SensorUnit.Percentage {
			v := s.Value
			r.Percent = &v
		} else if s.SensorUnit.BaseUnit == ipmi.SensorUnitType_RPM {
			v := s.Value
			r.RPM = &v
		}
		readings = append(readings, r)
	}
	return readings, nil
}

// SetSpeed sets the chassis fan speed to the given percentage [0,100].
// It uses Dell OEM IPMI raw commands to enable manual fan control and set the speed.
func (c *IPMIFanController) SetSpeed(percent int) error {
	if percent < 0 || percent > 100 {
		return fmt.Errorf("fan speed %d out of range [0,100]", percent)
	}

	ctx := context.Background()
	client, err := c.newClient()
	if err != nil {
		return fmt.Errorf("creating IPMI client: %w", err)
	}
	if err := client.Connect(ctx); err != nil {
		return fmt.Errorf("connecting to IPMI: %w", err)
	}
	defer client.Close(ctx)

	// Dell OEM: enable manual fan control
	if _, err := client.RawCommand(ctx, ipmi.NetFn(0x30), 0x30, []byte{0x01, 0x00}, ""); err != nil {
		return fmt.Errorf("enabling manual fan control: %w", err)
	}

	// Dell OEM: set fan speed
	if _, err := client.RawCommand(ctx, ipmi.NetFn(0x30), 0x30, []byte{0x02, 0xff, byte(percent)}, ""); err != nil {
		return fmt.Errorf("setting fan speed to %d%%: %w", percent, err)
	}
	return nil
}
