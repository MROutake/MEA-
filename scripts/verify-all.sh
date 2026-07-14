#!/usr/bin/env bash
# Führt alle lokal möglichen Qualitätsprüfungen aus; bricht beim ersten Fehler ab.
set -euo pipefail
cd "$(dirname "$0")"
./check-format.sh
./analyze-all.sh
./test-all.sh
./build-all.sh
echo "verify-all: ok"
