#!/usr/bin/env bash
# ESP32-Builds: Demo-Firmware und Embedded-Smoke-Test (nur kompilieren).
set -euo pipefail
cd "$(dirname "$0")/.."
echo "==> ESP32-Firmware"
(cd repositories/mea-demo-firmware && pio run -e esp32dev)
echo "==> Embedded-Smoke-Test (nur Build, kein Upload)"
(cd repositories/mea-demo-firmware && pio test -e esp32dev_test --without-uploading --without-testing)
echo "build-all: ok"
