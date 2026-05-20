package main

import (
	"testing"

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
	require.NoError(t, ctrl.SetSpeed(50))
}

func TestIPMIFanController_SetSpeedOutOfRange(t *testing.T) {
	mock := &mockIPMIClient{}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	assert.Error(t, ctrl.SetSpeed(101))
	assert.Error(t, ctrl.SetSpeed(-1))
}

func TestIPMIFanController_SetSpeedIPMIError(t *testing.T) {
	mock := &mockIPMIClient{rawErr: errIPMI("raw command failed")}
	ctrl := &IPMIFanController{newClient: mockIPMIFactory(mock)}
	assert.Error(t, ctrl.SetSpeed(50))
}

func TestIPMIFanController_SetSpeedConnectError(t *testing.T) {
	ctrl := &IPMIFanController{newClient: mockIPMIFactoryErr("no IPMI device")}
	assert.Error(t, ctrl.SetSpeed(50))
}
