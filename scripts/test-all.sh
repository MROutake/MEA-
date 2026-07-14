#!/usr/bin/env bash
# Native Unit-/Integrationstests aller Repositories (mit Sanitizern).
set -euo pipefail
cd "$(dirname "$0")/.."
for repo in repositories/mea-core repositories/mea-managers repositories/mea-processing \
            repositories/mea-device-analog-input repositories/mea-communication \
            repositories/mea-demo-firmware \
            repositories/mea-protocol  repositories/mea-output \
            repositories/mea-network-ws500 repositories/mea-network-core \
            repositories/mea-device-74hc595; do
    echo "==> pio test ${repo}"
    (cd "$repo" && pio test -e native)
done
echo "test-all: ok"
