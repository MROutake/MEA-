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

if ! git config --global user.name >/dev/null || ! git config --global user.email >/dev/null; then
  echo "Bitte zuerst Git-Identität setzen:"
  echo '  git config --global user.name "Theo Anders"'
  echo '  git config --global user.email "theoanders14@gmail.com"'
  exit 1
fi

for repo in "${REPOS[@]}"; do
  path="$ROOT/repositories/$repo"
  if [[ ! -d "$path/.git" ]]; then
    git -C "$path" init -b main
  fi

  if ! git -C "$path" rev-parse --verify HEAD >/dev/null 2>&1; then
    git -C "$path" add .
    git -C "$path" commit -m "chore: initial project scaffold"
  fi

done

echo "Alle lokalen Repositories sind initialisiert."
