#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPOS=(
  mea-core
  mea-device-analog-input
  mea-processing
  mea-communication
  mea-managers
  mea-state-machine
  mea-demo-firmware
)

for repo in "${REPOS[@]}"; do
  path="$ROOT/repositories/$repo"
  echo "==> $repo"
  git -C "$path" push -u origin main
  git -C "$path" push origin --tags
done
