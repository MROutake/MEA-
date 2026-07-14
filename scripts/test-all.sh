#!/usr/bin/env bash
# Native Unit-/Integrationstests aller Repositories (mit Sanitizern).
set -euo pipefail
cd "$(dirname "$0")/.."
for repo in repositories/mea-core repositories/mea-managers repositories/mea-processing \
            repositories/mea-device-analog-input repositories/mea-communication \
            repositories/mea-state-machine repositories/mea-demo-firmware; do
    echo "==> pio test ${repo}"
    (cd "$repo" && pio test -e native)
done
echo "test-all: ok"
