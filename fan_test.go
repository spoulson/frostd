package main

import (
	"errors"
	"testing"
)

func TestSuggestSpeed_AtIdealTemp(t *testing.T) {
	speed := SuggestSpeed(40, 40, 75)
	if speed != 0 {
		t.Errorf("SuggestSpeed at ideal = %d, want 0", speed)
	}
}

func TestSuggestSpeed_BelowIdealTemp(t *testing.T) {
	speed := SuggestSpeed(35, 40, 75)
	if speed != 0 {
		t.Errorf("SuggestSpeed below ideal = %d, want 0", speed)
	}
}

func TestSuggestSpeed_AtMaxTemp(t *testing.T) {
	speed := SuggestSpeed(75, 40, 75)
	if speed != 100 {
		t.Errorf("SuggestSpeed at max = %d, want 100", speed)
	}
}

func TestSuggestSpeed_AboveMaxTemp(t *testing.T) {
	speed := SuggestSpeed(90, 40, 75)
	if speed != 100 {
		t.Errorf("SuggestSpeed above max = %d, want 100", speed)
	}
}

func TestSuggestSpeed_Midpoint(t *testing.T) {
	// ideal=40, max=75 → range=35, midpoint actual=57.5
	// (57.5-40)^2 * (100/(75-40)^2) = 17.5^2 * (100/1225) = 306.25 * 0.08163... = 25
	speed := SuggestSpeed(57.5, 40, 75)
	if speed != 25 {
		t.Errorf("SuggestSpeed at midpoint = %d, want 25", speed)
	}
}

func TestSuggestSpeed_ThreeQuarterTemp(t *testing.T) {
	// actual = 40 + 0.75*35 = 66.25
	// (26.25)^2 * (100/1225) = 689.0625 * 0.081632... ≈ 56.25
	speed := SuggestSpeed(66.25, 40, 75)
	if speed != 56 {
		t.Errorf("SuggestSpeed at 3/4 = %d, want 56", speed)
	}
}

type mockRunner struct {
	calls  [][]string
	output []byte
	err    error
}

func (m *mockRunner) Run(name string, args ...string) ([]byte, error) {
	call := append([]string{name}, args...)
	m.calls = append(m.calls, call)
	return m.output, m.err
}

func TestIPMIFanController_SetSpeed(t *testing.T) {
	r := &mockRunner{}
	ctrl := &IPMIFanController{runner: r}

	if err := ctrl.SetSpeed(50); err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(r.calls) != 2 {
		t.Fatalf("expected 2 ipmitool calls, got %d", len(r.calls))
	}
	// First call: enable manual control — args: raw 0x30 0x30 0x01 0x00
	if r.calls[0][4] != "0x01" || r.calls[0][5] != "0x00" {
		t.Errorf("unexpected manual enable args: %v", r.calls[0])
	}
	// Second call: set speed 0x32 = 50
	last := r.calls[1]
	if last[len(last)-1] != "0x32" {
		t.Errorf("expected hex speed 0x32, got %s", last[len(last)-1])
	}
}

func TestIPMIFanController_SetSpeed_OutOfRange(t *testing.T) {
	r := &mockRunner{}
	ctrl := &IPMIFanController{runner: r}
	if err := ctrl.SetSpeed(101); err == nil {
		t.Error("expected error for speed > 100")
	}
	if err := ctrl.SetSpeed(-1); err == nil {
		t.Error("expected error for speed < 0")
	}
}

func TestIPMIFanController_SetSpeed_IPMIError(t *testing.T) {
	r := &mockRunner{err: errors.New("ipmitool not found")}
	ctrl := &IPMIFanController{runner: r}
	if err := ctrl.SetSpeed(50); err == nil {
		t.Error("expected error when ipmitool fails")
	}
}
