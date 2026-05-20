package main

import (
	"errors"
	"testing"
)

const sampleIPMIOutput = `Inlet Temp       | 04h | ok  | 7.1 | 23 degrees C
Exhaust Temp     | 01h | ok  | 7.1 | 29 degrees C
Temp             | 0Eh | ok  | 3.1 | 45 degrees C
Temp             | 0Fh | ok  | 3.2 | 50 degrees C
`

func TestCPUReader_ParsesProcessorTemps(t *testing.T) {
	temps, err := parseCPUTemps(sampleIPMIOutput)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(temps) != 2 {
		t.Fatalf("got %d temps, want 2", len(temps))
	}
	if temps[0] != 45 {
		t.Errorf("temps[0] = %.1f, want 45", temps[0])
	}
	if temps[1] != 50 {
		t.Errorf("temps[1] = %.1f, want 50", temps[1])
	}
}

func TestCPUReader_IgnoresNonProcessorSensors(t *testing.T) {
	output := `Inlet Temp       | 04h | ok  | 7.1 | 23 degrees C
Exhaust Temp     | 01h | ok  | 7.1 | 29 degrees C
`
	_, err := parseCPUTemps(output)
	if err == nil {
		t.Fatal("expected error when no processor sensors found")
	}
}

func TestCPUReader_EmptyOutput(t *testing.T) {
	_, err := parseCPUTemps("")
	if err == nil {
		t.Fatal("expected error for empty output")
	}
}

func TestCPUReader_ReadTemperatures_UsesIpmitool(t *testing.T) {
	r := &mockRunner{output: []byte(sampleIPMIOutput)}
	reader := &CPUReader{runner: r}
	temps, err := reader.ReadTemperatures()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(temps) != 2 {
		t.Errorf("got %d temps, want 2", len(temps))
	}
	if len(r.calls) != 1 || r.calls[0][1] != "sdr" {
		t.Errorf("unexpected ipmitool call: %v", r.calls)
	}
}

func TestCPUReader_ReadTemperatures_CommandError(t *testing.T) {
	r := &mockRunner{err: errors.New("ipmitool not found")}
	reader := &CPUReader{runner: r}
	_, err := reader.ReadTemperatures()
	if err == nil {
		t.Fatal("expected error when ipmitool fails")
	}
}
