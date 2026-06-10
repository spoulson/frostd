package main

import "os/exec"

type CommandRunner interface {
	Run(name string, args ...string) ([]byte, error)
}

type RealRunner struct{}

func (r RealRunner) Run(name string, args ...string) ([]byte, error) {
	return exec.Command(name, args...).Output()
}
