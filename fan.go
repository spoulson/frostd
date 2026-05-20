package main

import (
	"fmt"
	"math"
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
	runner CommandRunner
}

func (c *IPMIFanController) SetSpeed(percent int) error {
	if percent < 0 || percent > 100 {
		return fmt.Errorf("fan speed %d out of range [0,100]", percent)
	}

	// Enable manual fan control
	if _, err := c.runner.Run("ipmitool", "raw", "0x30", "0x30", "0x01", "0x00"); err != nil {
		return fmt.Errorf("enabling manual fan control: %w", err)
	}

	hex := fmt.Sprintf("0x%02x", percent)
	if _, err := c.runner.Run("ipmitool", "raw", "0x30", "0x30", "0x02", "0xff", hex); err != nil {
		return fmt.Errorf("setting fan speed to %d%%: %w", percent, err)
	}
	return nil
}
