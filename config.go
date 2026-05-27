package main

import (
	"fmt"
	"os"
	"time"

	"gopkg.in/yaml.v3"
)

type Config struct {
	LogFile        string        `yaml:"log_file"`
	DryRun         bool          `yaml:"dry_run"`
	FanLogInterval time.Duration `yaml:"fan_log_interval"`
	CPU            *SensorConfig `yaml:"cpu"`
	GPU            *SensorConfig `yaml:"gpu"`
}

type SensorConfig struct {
	IdealTemp      float64       `yaml:"ideal_temp"`
	MaxTemp        float64       `yaml:"max_temp"`
	SampleSize     int           `yaml:"sample_size"`
	SampleInterval time.Duration `yaml:"sample_interval"`
}

func loadConfig(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("reading config file: %w", err)
	}
	var cfg Config
	if err := yaml.Unmarshal(data, &cfg); err != nil {
		return nil, fmt.Errorf("parsing config file: %w", err)
	}
	applyDefaults(&cfg)
	if err := validateConfig(&cfg); err != nil {
		return nil, err
	}
	return &cfg, nil
}

func applyDefaults(cfg *Config) {
	if cfg.FanLogInterval == 0 {
		cfg.FanLogInterval = 15 * time.Second
	}
	if cfg.CPU != nil {
		applySensorDefaults(cfg.CPU)
	}
	if cfg.GPU != nil {
		applySensorDefaults(cfg.GPU)
	}
}

func applySensorDefaults(d *SensorConfig) {
	if d.IdealTemp == 0 {
		d.IdealTemp = 40
	}
	if d.MaxTemp == 0 {
		d.MaxTemp = 75
	}
	if d.SampleSize == 0 {
		d.SampleSize = 3
	}
	if d.SampleInterval == 0 {
		d.SampleInterval = 15 * time.Second
	}
}

func validateConfig(cfg *Config) error {
	if cfg.CPU == nil && cfg.GPU == nil {
		return fmt.Errorf("config must enable at least one sensor type (cpu or gpu)")
	}
	if cfg.FanLogInterval <= 0 {
		return fmt.Errorf("fan_log_interval must be positive, got %s", cfg.FanLogInterval)
	}
	if cfg.CPU != nil {
		if err := validateSensorConfig("cpu", cfg.CPU); err != nil {
			return err
		}
	}
	if cfg.GPU != nil {
		if err := validateSensorConfig("gpu", cfg.GPU); err != nil {
			return err
		}
	}
	return nil
}

func validateSensorConfig(name string, d *SensorConfig) error {
	if d.IdealTemp <= 0 {
		return fmt.Errorf("%s: ideal_temp must be positive, got %.1f", name, d.IdealTemp)
	}
	if d.MaxTemp <= 0 {
		return fmt.Errorf("%s: max_temp must be positive, got %.1f", name, d.MaxTemp)
	}
	if d.IdealTemp >= d.MaxTemp {
		return fmt.Errorf("%s: ideal_temp (%.1f) must be less than max_temp (%.1f)", name, d.IdealTemp, d.MaxTemp)
	}
	if d.SampleSize < 1 {
		return fmt.Errorf("%s: sample_size must be at least 1, got %d", name, d.SampleSize)
	}
	if d.SampleInterval <= 0 {
		return fmt.Errorf("%s: sample_interval must be positive, got %s", name, d.SampleInterval)
	}
	return nil
}
