package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"path/filepath"
	"syscall"
)

type args struct {
	configPath string
}

func parseArgs() args {
	configPath := flag.String("c", "/etc/frostd.yaml", "path to config file")
	flag.Parse()
	return args{configPath: *configPath}
}

func main() {
	a := parseArgs()

	cfg, err := loadConfig(a.configPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "frostd: configuration error: %v\n", err)
		os.Exit(1)
	}

	logger, closer, err := setupLogger(cfg.LogFile)
	if err != nil {
		fmt.Fprintf(os.Stderr, "frostd: failed to open log file: %v\n", err)
		os.Exit(1)
	}
	defer closer()

	fanCtrl := &IPMIFanController{newClient: newRealIPMIClient}

	var monitors []*SensorMonitor
	if cfg.CPU != nil {
		monitors = append(monitors, newSensorMonitor("cpu", cfg.CPU, &CPUReader{newClient: newRealIPMIClient}))
	}
	if cfg.GPU != nil {
		monitors = append(monitors, newSensorMonitor("gpu", cfg.GPU, &GPUReader{runner: RealRunner{}}))
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()

	logger.Info("frostd started", "config", a.configPath, "dry_run", cfg.DryRun)
	run(ctx, cfg, fanCtrl, monitors, logger)
	logger.Info("frostd stopping")
}

func setupLogger(logFile string) (*slog.Logger, func(), error) {
	if logFile == "" {
		return slog.New(slog.NewTextHandler(os.Stdout, nil)), func() {}, nil
	}
	if err := os.MkdirAll(filepath.Dir(logFile), 0o755); err != nil {
		return nil, nil, err
	}
	f, err := os.OpenFile(logFile, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0o644)
	if err != nil {
		return nil, nil, err
	}
	logger := slog.New(slog.NewTextHandler(f, nil))
	return logger, func() { f.Close() }, nil
}
