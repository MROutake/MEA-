#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Verwendung: $0 <ssh-host> <server-verzeichnis>"
  echo "Beispiel:   $0 git@gitserver.local /srv/git/embedded"
  exit 2
fi

HOST="$1"
BASE="$2"
REPOS=(
  mea-core
  mea-device-analog-input
  mea-processing
  mea-communication
  mea-managers
  mea-state-machine
  mea-demo-firmware
)

printf -v quoted_base '%q' "$BASE"
remote_script="set -e; mkdir -p $quoted_base;"
for repo in "${REPOS[@]}"; do
  printf -v qrepo '%q' "$repo.git"
  remote_script+=" if [ ! -d $quoted_base/$qrepo ]; then git init --bare $quoted_base/$qrepo; fi;"
done

ssh "$HOST" "$remote_script"
echo "Bare Repositories auf $HOST:$BASE sind angelegt."
