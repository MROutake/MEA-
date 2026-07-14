#!/usr/bin/env bash
# Statische Analyse (cppcheck) über die src/-Verzeichnisse aller Repositories.
# Header werden explizit übergeben (mehrere Libraries sind header-only).
set -euo pipefail
cd "$(dirname "$0")/.."
for repo in repositories/*/; do
    [ -d "${repo}src" ] || continue
    echo "==> cppcheck ${repo}"
    find "${repo}src" -type f \( -name '*.cpp' -o -name '*.h' \) -print0 |
        xargs -0 cppcheck --std=c++17 --language=c++ --inline-suppr \
            --enable=warning,style,performance,portability \
            --error-exitcode=1 --quiet \
            --suppress=missingIncludeSystem \
            --suppress=unusedStructMember \
            -I "${repo}src"
done
echo "analyze-all: ok"
