package main

import (
	"errors"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestGPUReader_ParsesSingleGPU(t *testing.T) {
	temps, err := parseGPUTemps("72\n")
	require.NoError(t, err)
	assert.Equal(t, map[string]float64{"Temp": 72}, temps)
}

func TestGPUReader_ParsesMultipleGPUs(t *testing.T) {
	temps, err := parseGPUTemps("68\n74\n")
	require.NoError(t, err)
	assert.Equal(t, map[string]float64{"Temp": 68, "Temp_2": 74}, temps)
}

func TestGPUReader_EmptyOutput(t *testing.T) {
	_, err := parseGPUTemps("")
	assert.Error(t, err)
}

func TestGPUReader_InvalidLine(t *testing.T) {
	_, err := parseGPUTemps("not a number\n")
	assert.Error(t, err)
}

func TestGPUReader_ReadTemperatures_CallsNvidiaSmi(t *testing.T) {
	r := &mockRunner{output: []byte("72\n")}
	reader := &GPUReader{runner: r}
	temps, err := reader.ReadTemperatures()
	require.NoError(t, err)
	assert.Equal(t, map[string]float64{"Temp": 72}, temps)
}

func TestGPUReader_ReadTemperatures_CommandError(t *testing.T) {
	r := &mockRunner{err: errors.New("nvidia-smi not found")}
	reader := &GPUReader{runner: r}
	_, err := reader.ReadTemperatures()
	assert.Error(t, err)
}
