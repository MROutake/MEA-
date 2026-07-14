#!/usr/bin/env bash
# Prüft die Formatierung aller C++-Quellen (Exit-Code != 0 bei Abweichungen).
set -euo pipefail
cd "$(dirname "$0")/.."
find repositories -path '*/.pio' -prune -o \
    -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) -print0 |
    xargs -0 clang-format --dry-run --Werror
echo "check-format: ok"
