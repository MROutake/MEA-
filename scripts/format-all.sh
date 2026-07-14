#!/usr/bin/env bash
# Formatiert alle C++-Quellen des Workspace mit clang-format (in-place).
set -euo pipefail
cd "$(dirname "$0")/.."
find repositories -path '*/.pio' -prune -o \
    -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) -print0 |
    xargs -0 clang-format -i
echo "format-all: ok"
