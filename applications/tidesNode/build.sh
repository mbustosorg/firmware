#!/bin/sh
set -x
rm firmware*
spark compile tidesNode.ino
spark flash --usb firmware_*.bin
