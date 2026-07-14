#!/usr/bin/env bash
set -euo pipefail

sudo pacman -Syu
sudo pacman -S --needed \
  git base-devel python \
  platformio-core platformio-core-udev \
  clang cppcheck

cat <<'EOF'

Systemwerkzeuge wurden installiert.

Für Microsoft VS Code mit der offiziellen Extension-Gallery:
  paru -S visual-studio-code-bin
oder:
  yay -S visual-studio-code-bin

Danach:
  code --install-extension platformio.platformio-ide
  code --install-extension ms-vscode.cpptools
  code --install-extension eamodio.gitlens

Board anschließend neu verbinden und mit "pio device list" prüfen.
EOF
