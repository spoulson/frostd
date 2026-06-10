package main

import (
	"context"

	ipmi "github.com/bougou/go-ipmi"
)

type ipmiClient interface {
	Connect(ctx context.Context) error
	Close(ctx context.Context) error
	GetSensors(ctx context.Context, opts ...ipmi.SensorFilterOption) ([]*ipmi.Sensor, error)
	RawCommand(ctx context.Context, netFn ipmi.NetFn, cmd uint8, data []byte, name string) (*ipmi.CommandRawResponse, error)
}

func newRealIPMIClient() (ipmiClient, error) {
	return ipmi.NewOpenClient()
}
