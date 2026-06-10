package main

import (
	"context"
	"errors"

	ipmi "github.com/bougou/go-ipmi"
)

// mockIPMIClient implements ipmiClient for tests.
type mockIPMIClient struct {
	sensors    []*ipmi.Sensor
	rawErr     error
	connectErr error
}

func (m *mockIPMIClient) Connect(_ context.Context) error { return m.connectErr }
func (m *mockIPMIClient) Close(_ context.Context) error   { return nil }

func (m *mockIPMIClient) GetSensors(_ context.Context, _ ...ipmi.SensorFilterOption) ([]*ipmi.Sensor, error) {
	if m.rawErr != nil {
		return nil, m.rawErr
	}
	return m.sensors, nil
}

func (m *mockIPMIClient) RawCommand(_ context.Context, _ ipmi.NetFn, _ uint8, _ []byte, _ string) (*ipmi.CommandRawResponse, error) {
	if m.rawErr != nil {
		return nil, m.rawErr
	}
	return &ipmi.CommandRawResponse{}, nil
}

func mockIPMIFactory(mock *mockIPMIClient) func() (ipmiClient, error) {
	return func() (ipmiClient, error) { return mock, nil }
}

func mockIPMIFactoryErr(err string) func() (ipmiClient, error) {
	return func() (ipmiClient, error) { return nil, errors.New(err) }
}

// mockRunner implements CommandRunner for tests.
type mockRunner struct {
	output []byte
	err    error
}

func (m *mockRunner) Run(_ string, _ ...string) ([]byte, error) {
	return m.output, m.err
}
