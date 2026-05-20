package main

import (
	"os"
	"testing"
	"time"
)

func writeTempConfig(t *testing.T, content string) string {
	t.Helper()
	f, err := os.CreateTemp(t.TempDir(), "frostd-*.yaml")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := f.WriteString(content); err != nil {
		t.Fatal(err)
	}
	f.Close()
	return f.Name()
}

func TestLoadConfig_HappyPath(t *testing.T) {
	path := writeTempConfig(t, `
log_file: /var/log/frostd/frostd.log
cpu:
  ideal_temp: 45
  max_temp: 80
  sample_size: 5
  sample_interval: 10s
gpu:
  ideal_temp: 50
  max_temp: 85
  sample_size: 4
  sample_interval: 20s
`)
	cfg, err := loadConfig(path)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if cfg.LogFile != "/var/log/frostd/frostd.log" {
		t.Errorf("LogFile = %q, want /var/log/frostd/frostd.log", cfg.LogFile)
	}
	if cfg.CPU.IdealTemp != 45 {
		t.Errorf("CPU.IdealTemp = %.1f, want 45", cfg.CPU.IdealTemp)
	}
	if cfg.CPU.MaxTemp != 80 {
		t.Errorf("CPU.MaxTemp = %.1f, want 80", cfg.CPU.MaxTemp)
	}
	if cfg.CPU.SampleSize != 5 {
		t.Errorf("CPU.SampleSize = %d, want 5", cfg.CPU.SampleSize)
	}
	if cfg.CPU.SampleInterval != 10*time.Second {
		t.Errorf("CPU.SampleInterval = %v, want 10s", cfg.CPU.SampleInterval)
	}
	if cfg.GPU.IdealTemp != 50 {
		t.Errorf("GPU.IdealTemp = %.1f, want 50", cfg.GPU.IdealTemp)
	}
}

func TestLoadConfig_Defaults(t *testing.T) {
	path := writeTempConfig(t, `
cpu: {}
`)
	cfg, err := loadConfig(path)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if cfg.CPU.IdealTemp != 40 {
		t.Errorf("CPU.IdealTemp = %.1f, want 40", cfg.CPU.IdealTemp)
	}
	if cfg.CPU.MaxTemp != 75 {
		t.Errorf("CPU.MaxTemp = %.1f, want 75", cfg.CPU.MaxTemp)
	}
	if cfg.CPU.SampleSize != 3 {
		t.Errorf("CPU.SampleSize = %d, want 3", cfg.CPU.SampleSize)
	}
	if cfg.CPU.SampleInterval != 15*time.Second {
		t.Errorf("CPU.SampleInterval = %v, want 15s", cfg.CPU.SampleInterval)
	}
}

func TestLoadConfig_NoDevices(t *testing.T) {
	path := writeTempConfig(t, `log_file: /tmp/test.log`)
	_, err := loadConfig(path)
	if err == nil {
		t.Fatal("expected error for config with no device types")
	}
}

func TestLoadConfig_IdealTempEqualMaxTemp(t *testing.T) {
	path := writeTempConfig(t, `
cpu:
  ideal_temp: 75
  max_temp: 75
`)
	_, err := loadConfig(path)
	if err == nil {
		t.Fatal("expected error when ideal_temp == max_temp")
	}
}

func TestLoadConfig_IdealTempGreaterThanMaxTemp(t *testing.T) {
	path := writeTempConfig(t, `
cpu:
  ideal_temp: 80
  max_temp: 75
`)
	_, err := loadConfig(path)
	if err == nil {
		t.Fatal("expected error when ideal_temp > max_temp")
	}
}

func TestLoadConfig_SampleSizeNegative(t *testing.T) {
	path := writeTempConfig(t, `
cpu:
  ideal_temp: 40
  max_temp: 75
  sample_size: -1
  sample_interval: 15s
`)
	_, err := loadConfig(path)
	if err == nil {
		t.Fatal("expected error for negative sample_size")
	}
}

func TestLoadConfig_NegativeSampleInterval(t *testing.T) {
	path := writeTempConfig(t, `
cpu:
  ideal_temp: 40
  max_temp: 75
  sample_size: 3
  sample_interval: -5s
`)
	_, err := loadConfig(path)
	if err == nil {
		t.Fatal("expected error for negative sample_interval")
	}
}

func TestLoadConfig_InvalidYAML(t *testing.T) {
	path := writeTempConfig(t, `:::invalid yaml:::`)
	_, err := loadConfig(path)
	if err == nil {
		t.Fatal("expected error for invalid YAML")
	}
}

func TestLoadConfig_MissingFile(t *testing.T) {
	_, err := loadConfig("/nonexistent/path/frostd.yaml")
	if err == nil {
		t.Fatal("expected error for missing file")
	}
}
