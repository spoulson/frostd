#!/bin/sh
# Set automatic fan control mode.
# Run as root.

ipmitool -I open raw 0x30 0x30 0x01 0x01
