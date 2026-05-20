package main

import (
	"errors"
	"testing"
)

func TestGPUReader_ParsesSingleGPU(t *testing.T) {
	temps, err := parseGPUTemps("72\n")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(temps) != 1 || temps[0] != 72 {
		t.Errorf("temps = %v, want [72]", temps)
	}
}

func TestGPUReader_ParsesMultipleGPUs(t *testing.T) {
	temps, err := parseGPUTemps("68\n74\n")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(temps) != 2 {
		t.Fatalf("got %d temps, want 2", len(temps))
	}
	if temps[0] != 68 || temps[1] != 74 {
		t.Errorf("temps = %v, want [68 74]", temps)
	}
}

func TestGPUReader_EmptyOutput(t *testing.T) {
	_, err := parseGPUTemps("")
	if err == nil {
		t.Fatal("expected error for empty output")
	}
}

func TestGPUReader_InvalidLine(t *testing.T) {
	_, err := parseGPUTemps("not a number\n")
	if err == nil {
		t.Fatal("expected error for non-numeric output")
	}
}

func TestGPUReader_ReadTemperatures_CallsNvidiaSmi(t *testing.T) {
	r := &mockRunner{output: []byte("72\n")}
	reader := &GPUReader{runner: r}
	temps, err := reader.ReadTemperatures()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(temps) != 1 || temps[0] != 72 {
		t.Errorf("temps = %v, want [72]", temps)
	}
	if len(r.calls) != 1 || r.calls[0][0] != "nvidia-smi" {
		t.Errorf("unexpected command call: %v", r.calls)
	}
}

func TestGPUReader_ReadTemperatures_CommandError(t *testing.T) {
	r := &mockRunner{err: errors.New("nvidia-smi not found")}
	reader := &GPUReader{runner: r}
	_, err := reader.ReadTemperatures()
	if err == nil {
		t.Fatal("expected error when nvidia-smi fails")
	}
}
