#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "Verwendung: $0 <gitea-url> <owner> <user|org> <private|public>"
  echo "Beispiel:   $0 'http://192.168.178.99:3000' 'Theo' user private"
  exit 2
fi

if [[ -z "${GITEA_TOKEN:-}" ]]; then
  echo "Fehlt: GITEA_TOKEN ist nicht gesetzt." >&2
  echo 'Für zsh: read -s "GITEA_TOKEN?Gitea-Token: "; echo; export GITEA_TOKEN' >&2
  exit 1
fi

BASE_URL="${1%/}"
OWNER="$2"
OWNER_TYPE="$3"
VISIBILITY="$4"
API_URL="$BASE_URL/api/v1"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

case "$OWNER_TYPE" in
  user)
    CREATE_ENDPOINT="$API_URL/user/repos"
    ;;
  org)
    CREATE_ENDPOINT="$API_URL/orgs/$OWNER/repos"
    ;;
  *)
    echo "Ungueltiger Owner-Typ: $OWNER_TYPE (erlaubt: user, org)" >&2
    exit 2
    ;;
esac

case "$VISIBILITY" in
  private)
    PRIVATE=true
    ;;
  public)
    PRIVATE=false
    ;;
  *)
    echo "Ungueltige Sichtbarkeit: $VISIBILITY (erlaubt: private, public)" >&2
    exit 2
    ;;
esac

REPOS=(
  mea-core
  mea-device-analog-input
  mea-processing
  mea-communication
  mea-managers
  mea-state-machine
  mea-demo-firmware
)

create_payload() {
  local name="$1"
  printf '{"name":"%s","private":%s,"auto_init":false}' "$name" "$PRIVATE"
}

api_status() {
  local method="$1"
  local url="$2"
  local data="${3:-}"

  if [[ -n "$data" ]]; then
    curl -sS -o /tmp/create-gitea-repositories.response -w '%{http_code}' \
      -X "$method" \
      -H "Authorization: token $GITEA_TOKEN" \
      -H "Content-Type: application/json" \
      --data "$data" \
      "$url"
  else
    curl -sS -o /tmp/create-gitea-repositories.response -w '%{http_code}' \
      -X "$method" \
      -H "Authorization: token $GITEA_TOKEN" \
      "$url"
  fi
}

for repo in "${REPOS[@]}"; do
  existing_status="$(api_status GET "$API_URL/repos/$OWNER/$repo")"

  if [[ "$existing_status" == "200" ]]; then
    printf '%-30s vorhanden\n' "$repo"
  else
    create_status="$(api_status POST "$CREATE_ENDPOINT" "$(create_payload "$repo")")"

    case "$create_status" in
      200|201)
        printf '%-30s angelegt\n' "$repo"
        ;;
      409)
        printf '%-30s vorhanden\n' "$repo"
        ;;
      *)
        echo "Fehler beim Anlegen von $repo (HTTP $create_status)." >&2
        echo "Antwort von Gitea:" >&2
        sed 's/^/  /' /tmp/create-gitea-repositories.response >&2
        exit 1
        ;;
    esac
  fi

  path="$ROOT/repositories/$repo"
  remote_url="$BASE_URL/$OWNER/$repo.git"

  if [[ ! -d "$path/.git" ]]; then
    echo "Fehlt: $path/.git – zuerst scripts/init-repositories.sh ausführen." >&2
    exit 1
  fi

  if git -C "$path" remote get-url origin >/dev/null 2>&1; then
    git -C "$path" remote set-url origin "$remote_url"
  else
    git -C "$path" remote add origin "$remote_url"
  fi
done

echo "Gitea-Repositories fuer $OWNER sind bereit."
echo "Remote origin wurde in allen lokalen Repositories gesetzt."
