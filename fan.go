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

type FanController interface {
	SetSpeed(percent int) error
}

type IPMIFanController struct {
	newClient func() (ipmiClient, error)
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
