package main

import (
	"testing"

	ipmi "github.com/bougou/go-ipmi"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func makeProcessorTempSensor(name string, value float64, entityInstance uint8) *ipmi.Sensor {
	return &ipmi.Sensor{
		Name:           name,
		SensorType:     ipmi.SensorTypeTemperature,
		EntityID:       ipmi.EntityID(0x03),
		EntityInstance: ipmi.EntityInstance(entityInstance),
		Value:          value,
	}
}

func makeInletTempSensor(value float64) *ipmi.Sensor {
	return &ipmi.Sensor{
		SensorType: ipmi.SensorTypeTemperature,
		EntityID:   ipmi.EntityID(0x07), // system board
		Value:      value,
	}
}

func TestCPUReader_ReturnsProcessorTemps(t *testing.T) {
	mock := &mockIPMIClient{
		sensors: []*ipmi.Sensor{
			makeProcessorTempSensor("CPU1 Temp", 45, 1),
			makeProcessorTempSensor("CPU2 Temp", 50, 2),
			makeInletTempSensor(23),
		},
	}
	reader := &CPUReader{newClient: mockIPMIFactory(mock)}
	temps, err := reader.ReadTemperatures()
	require.NoError(t, err)
	assert.Equal(t, map[string]float64{"CPU1 Temp": 45, "CPU2 Temp": 50}, temps)
}

func TestCPUReader_DeduplicatesSensorNames(t *testing.T) {
	mock := &mockIPMIClient{
		sensors: []*ipmi.Sensor{
			makeProcessorTempSensor("Temp", 45, 1),
			makeProcessorTempSensor("Temp", 50, 2),
			makeProcessorTempSensor("Temp", 55, 3),
		},
	}
	reader := &CPUReader{newClient: mockIPMIFactory(mock)}
	temps, err := reader.ReadTemperatures()
	require.NoError(t, err)
	assert.Equal(t, map[string]float64{"Temp": 45, "Temp_2": 50, "Temp_3": 55}, temps)
}

func TestCPUReader_IgnoresNonProcessorSensors(t *testing.T) {
	mock := &mockIPMIClient{
		sensors: []*ipmi.Sensor{
			makeInletTempSensor(23),
		},
	}
	reader := &CPUReader{newClient: mockIPMIFactory(mock)}
	_, err := reader.ReadTemperatures()
	assert.ErrorContains(t, err, "no CPU temperature sensors found")
}

func TestCPUReader_EmptySensorList(t *testing.T) {
	mock := &mockIPMIClient{sensors: []*ipmi.Sensor{}}
	reader := &CPUReader{newClient: mockIPMIFactory(mock)}
	_, err := reader.ReadTemperatures()
	assert.Error(t, err)
}

func TestCPUReader_ClientConnectError(t *testing.T) {
	reader := &CPUReader{newClient: mockIPMIFactoryErr("no IPMI device")}
	_, err := reader.ReadTemperatures()
	assert.Error(t, err)
}

func TestCPUReader_GetSensorsError(t *testing.T) {
	mock := &mockIPMIClient{rawErr: errIPMI("ipmitool failed")}
	reader := &CPUReader{newClient: mockIPMIFactory(mock)}
	_, err := reader.ReadTemperatures()
	assert.Error(t, err)
}

// errIPMI is a simple error helper for tests.
type errIPMI string

func (e errIPMI) Error() string { return string(e) }
