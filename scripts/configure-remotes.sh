#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Verwendung: $0 <remote-basis>"
  echo "Beispiel:   $0 'git@gitserver.local:/srv/git/embedded'"
  echo "Beispiel:   $0 'git@gitserver.local:theo'"
  exit 2
fi

BASE="${1%/}"
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
  url="$BASE/$repo.git"

  if [[ ! -d "$path/.git" ]]; then
    echo "Fehlt: $path/.git – zuerst scripts/init-repositories.sh ausführen." >&2
    exit 1
  fi

  if git -C "$path" remote get-url origin >/dev/null 2>&1; then
    git -C "$path" remote set-url origin "$url"
  else
    git -C "$path" remote add origin "$url"
  fi

  printf '%-30s %s\n' "$repo" "$url"
done
