package main

import (
	"testing"

	ipmi "github.com/bougou/go-ipmi"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestSuggestSpeed_AtIdealTemp(t *testing.T) {
	assert.Equal(t, 0, SuggestSpeed(40, 40, 75))
}

func TestSuggestSpeed_BelowIdealTemp(t *testing.T) {
	assert.Equal(t, 0, SuggestSpeed(35, 40, 75))
}

func TestSuggestSpeed_AtMaxTemp(t *testing.T) {
	assert.Equal(t, 100, SuggestSpeed(75, 40, 75))
}

func TestSuggestSpeed_AboveMaxTemp(t *testing.T) {
	assert.Equal(t, 100, SuggestSpeed(90, 40, 75))
}

func TestSuggestSpeed_Midpoint(t *testing.T) {
	// ideal=40, max=75, actual=57.5 → (17.5)^2 * (100/1225) = 25
	assert.Equal(t, 25, SuggestSpeed(57.5, 40, 75))
}

func TestSuggestSpeed_ThreeQuarterTemp(t *testing.T) {
	// actual = 40 + 0.75*35 = 66.25 → (26.25)^2 * (100/1225) ≈ 56.25 → rounds to 56
	assert.Equal(t, 56, SuggestSpeed(66.25, 40, 75))
}

func TestIPMIFanController_SetSpeed(t *testing.T) {
	mock := &mockIPMIClient{}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	require.NoError(t, ctrl.SetSpeed(t.Context(), 50))
}

func TestIPMIFanController_SetSpeedOutOfRange(t *testing.T) {
	mock := &mockIPMIClient{}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	assert.Error(t, ctrl.SetSpeed(t.Context(), 101))
	assert.Error(t, ctrl.SetSpeed(t.Context(), -1))
}

func TestIPMIFanController_SetSpeedIPMIError(t *testing.T) {
	mock := &mockIPMIClient{rawErr: errIPMI("raw command failed")}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	assert.Error(t, ctrl.SetSpeed(t.Context(), 50))
}

func TestIPMIFanController_SetSpeedConnectError(t *testing.T) {
	ctrl := &IPMIFanController{newClient: mockIPMIFactoryErr("no IPMI device")}
	assert.Error(t, ctrl.SetSpeed(t.Context(), 50))
}

func ptr[T any](v T) *T { return &v }

func TestIPMIFanController_ReadFanSpeeds_RPM(t *testing.T) {
	mock := &mockIPMIClient{
		sensors: []*ipmi.Sensor{
			{
				Name:             "Fan1",
				SensorType:       ipmi.SensorTypeFan,
				HasAnalogReading: true,
				Value:            3600,
				SensorUnit:       ipmi.SensorUnit{BaseUnit: ipmi.SensorUnitType_RPM},
			},
		},
	}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	readings, err := ctrl.ReadFanSpeeds(t.Context())
	require.NoError(t, err)
	require.Len(t, readings, 1)
	assert.Equal(t, "Fan1", readings[0].Name)
	assert.Equal(t, ptr(3600.0), readings[0].RPM)
	assert.Nil(t, readings[0].Percent)
}

func TestIPMIFanController_ReadFanSpeeds_Percent(t *testing.T) {
	mock := &mockIPMIClient{
		sensors: []*ipmi.Sensor{
			{
				Name:             "Fan1",
				SensorType:       ipmi.SensorTypeFan,
				HasAnalogReading: true,
				Value:            50,
				SensorUnit:       ipmi.SensorUnit{Percentage: true},
			},
		},
	}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	readings, err := ctrl.ReadFanSpeeds(t.Context())
	require.NoError(t, err)
	require.Len(t, readings, 1)
	assert.Equal(t, ptr(50.0), readings[0].Percent)
	assert.Nil(t, readings[0].RPM)
}

func TestIPMIFanController_ReadFanSpeeds_SkipsDiscrete(t *testing.T) {
	mock := &mockIPMIClient{
		sensors: []*ipmi.Sensor{
			{Name: "Fan1", SensorType: ipmi.SensorTypeFan, HasAnalogReading: false},
		},
	}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	readings, err := ctrl.ReadFanSpeeds(t.Context())
	require.NoError(t, err)
	assert.Empty(t, readings)
}

func TestIPMIFanController_ReadFanSpeeds_IPMIError(t *testing.T) {
	mock := &mockIPMIClient{rawErr: errIPMI("sensor read failed")}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	_, err := ctrl.ReadFanSpeeds(t.Context())
	assert.Error(t, err)
}

func TestIPMIFanController_ReadFanSpeeds_ConnectError(t *testing.T) {
	ctrl := &IPMIFanController{newClient: mockIPMIFactoryErr("no IPMI device")}
	_, err := ctrl.ReadFanSpeeds(t.Context())
	assert.Error(t, err)
}
