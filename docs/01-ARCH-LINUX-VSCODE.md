# Professionelle Einrichtung unter Arch Linux und VS Code

## 1. System aktualisieren

```bash
sudo pacman -Syu
```

## 2. Werkzeuge installieren

Arch Linux stellt PlatformIO Core und die passenden udev-Regeln in den offiziellen Repositories bereit:

```bash
sudo pacman -S --needed \
  git base-devel python \
  platformio-core platformio-core-udev \
  clang cppcheck
```

Prüfen:

```bash
git --version
pio --version
cppcheck --version
clang-tidy --version
```

Die VS-Code-Erweiterung bringt ebenfalls einen PlatformIO Core mit. Für reproduzierbare Terminal-, Skript- und CI-Aufrufe wird hier zusätzlich das Arch-Paket `platformio-core` genutzt.

## 3. VS Code installieren

Für die offizielle Microsoft-Erweiterungsgalerie ist die Microsoft-Ausgabe von VS Code am unkompliziertesten. Auf Arch Linux wird sie üblicherweise über das AUR-Paket `visual-studio-code-bin` installiert.

Mit `paru`:

```bash
paru -S visual-studio-code-bin
```

Mit `yay`:

```bash
yay -S visual-studio-code-bin
```

Ohne AUR-Helfer:

```bash
git clone https://aur.archlinux.org/visual-studio-code-bin.git
cd visual-studio-code-bin
less PKGBUILD
makepkg -si
```

AUR-PKGBUILDs sollten vor der Installation geprüft werden. Alternativ kann `code`/Code OSS aus den offiziellen Arch-Repositories verwendet werden; dabei kann die Verfügbarkeit von Marketplace-Erweiterungen abweichen.

## 4. VS-Code-Erweiterungen

```bash
code --install-extension platformio.platformio-ide
code --install-extension ms-vscode.cpptools
code --install-extension eamodio.gitlens
```

Danach:

```bash
code MEA-Embedded.code-workspace
```

Falls IntelliSense eine neue Library nicht findet:

1. Befehlspalette öffnen.
2. `PlatformIO: Rebuild C/C++ Project Index` ausführen.
3. Notfalls `pio run -t clean` und anschließend erneut bauen.

## 5. USB- und serielle Rechte

Das Paket `platformio-core-udev` installiert Regeln für unterstützte Boards. Danach das Board abziehen und neu verbinden.

Prüfen:

```bash
pio device list
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Falls Regeln manuell neu geladen werden müssen:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## 6. Git-Identität und SSH

```bash
git config --global user.name "Theo Anders"
git config --global user.email "theoanders14@gmail.com"
git config --global init.defaultBranch main
```

SSH-Schlüssel erzeugen:

```bash
ssh-keygen -t ed25519 -a 100 -C "theoanders14@gmail.com"
cat ~/.ssh/id_ed25519.pub
```

Den öffentlichen Schlüssel auf dem Git-Server hinterlegen und testen:

```bash
ssh -T git@gitserver.local
```

## 7. Workspace vorbereiten

```bash
./scripts/init-repositories.sh
code MEA-Embedded.code-workspace
```

## 8. Standardbefehle

```bash
cd repositories/mea-demo-firmware

# Tests auf dem Entwicklungs-PC
pio test -e native

# Firmware bauen
pio run -e esp32dev

# Firmware flashen
pio run -e esp32dev -t upload

# Seriellen Monitor öffnen
pio device monitor -b 115200

# Abhängigkeiten anzeigen
pio pkg list -e esp32dev

# Statische Analyse
pio check -e esp32dev
```

## 9. Board wechseln

In `platformio.ini` wird das Board über `board = esp32dev` festgelegt. Für ein anderes Board:

```bash
pio boards esp32
```

Danach `board`, Pinbelegung und eventuell ADC-Auflösung in `include/AppConfig.h` anpassen.

## 10. Empfohlener Arbeitsablauf

```text
Feature-Branch in betroffener Library
    -> Native-Tests
    -> Demo-Firmware bauen
    -> Test auf Hardware
    -> Commit
    -> SemVer-Version erhöhen
    -> Tag erstellen
    -> Push zum lokalen Git-Server
```

Offizielle Referenzen:

- https://docs.platformio.org/en/latest/integration/ide/vscode.html
- https://docs.platformio.org/en/latest/core/installation/udev-rules.html
- https://docs.platformio.org/en/latest/advanced/unit-testing/index.html
- https://docs.platformio.org/en/latest/librarymanager/dependencies.html
