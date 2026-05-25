package main

import (
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func writeTempConfig(t *testing.T, content string) string {
	t.Helper()
	f, err := os.CreateTemp(t.TempDir(), "frostd-*.yaml")
	require.NoError(t, err)
	_, err = f.WriteString(content)
	require.NoError(t, err)
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
	require.NoError(t, err)
	assert.Equal(t, "/var/log/frostd/frostd.log", cfg.LogFile)
	assert.False(t, cfg.DryRun)
	assert.Equal(t, 45.0, cfg.CPU.IdealTemp)
	assert.Equal(t, 80.0, cfg.CPU.MaxTemp)
	assert.Equal(t, 5, cfg.CPU.SampleSize)
	assert.Equal(t, 10*time.Second, cfg.CPU.SampleInterval)
	assert.Equal(t, 50.0, cfg.GPU.IdealTemp)
}

func TestLoadConfig_DryRunEnabled(t *testing.T) {
	path := writeTempConfig(t, `
dry_run: true
cpu:
  ideal_temp: 40
  max_temp: 75
`)
	cfg, err := loadConfig(path)
	require.NoError(t, err)
	assert.True(t, cfg.DryRun)
}

func TestLoadConfig_DryRunDefault(t *testing.T) {
	path := writeTempConfig(t, `cpu: {}`)
	cfg, err := loadConfig(path)
	require.NoError(t, err)
	assert.False(t, cfg.DryRun)
}

func TestLoadConfig_Defaults(t *testing.T) {
	path := writeTempConfig(t, `cpu: {}`)
	cfg, err := loadConfig(path)
	require.NoError(t, err)
	assert.Equal(t, 40.0, cfg.CPU.IdealTemp)
	assert.Equal(t, 75.0, cfg.CPU.MaxTemp)
	assert.Equal(t, 3, cfg.CPU.SampleSize)
	assert.Equal(t, 15*time.Second, cfg.CPU.SampleInterval)
}

func TestLoadConfig_NoDevices(t *testing.T) {
	path := writeTempConfig(t, `log_file: /tmp/test.log`)
	_, err := loadConfig(path)
	assert.ErrorContains(t, err, "at least one device type")
}

func TestLoadConfig_IdealTempEqualMaxTemp(t *testing.T) {
	path := writeTempConfig(t, `
cpu:
  ideal_temp: 75
  max_temp: 75
`)
	_, err := loadConfig(path)
	assert.Error(t, err)
}

func TestLoadConfig_IdealTempGreaterThanMaxTemp(t *testing.T) {
	path := writeTempConfig(t, `
cpu:
  ideal_temp: 80
  max_temp: 75
`)
	_, err := loadConfig(path)
	assert.Error(t, err)
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
	assert.ErrorContains(t, err, "sample_size")
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
	assert.ErrorContains(t, err, "sample_interval")
}

func TestLoadConfig_InvalidYAML(t *testing.T) {
	path := writeTempConfig(t, `:::invalid yaml:::`)
	_, err := loadConfig(path)
	assert.Error(t, err)
}

func TestLoadConfig_MissingFile(t *testing.T) {
	_, err := loadConfig("/nonexistent/path/frostd.yaml")
	assert.Error(t, err)
}
