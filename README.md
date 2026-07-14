# MEA Embedded Workspace


MEA steht für **Modular Embedded Architecture**. Dieser Workspace ist ein startfähiges Beispiel für eine professionelle, modulare Arduino-/PlatformIO-Architektur mit getrennten Git-Repositories.

## Enthaltene Repositories

| Repository | Aufgabe |
|---|---|
| `mea-core` | Gemeinsame Datentypen, Statuscodes und Interfaces |
| `mea-device-analog-input` | Beispiel für eine eigenständige Hardware-Library |
| `mea-processing` | Messwertverarbeitung ohne Hardwareabhängigkeit |
| `mea-communication` | Ausgabe standardisierter Messwerte über `Stream`/Serial |
| `mea-managers` | Registries/Manager mit fester Kapazität und ohne Heap-Allokation |
| `mea-state-machine` | Nicht blockierende Messwert-Pipeline als Zustandsmaschine |
| `mea-demo-firmware` | Echtes PlatformIO-Projekt für ein ESP32 DevKit plus Native-Tests |

## Schnellstart unter Arch Linux

```bash
cd MEA-Embedded-Workspace
./scripts/install-arch-tools.sh
./scripts/init-repositories.sh
code MEA-Embedded.code-workspace
```

Anschließend im PlatformIO-Terminal:

```bash
cd repositories/mea-demo-firmware
pio test -e native
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Der Beispielcode liest am ESP32 den ADC-Pin GPIO 34, verarbeitet den Rohwert zu einer Spannung und sendet ein CSV-Messwertpaket über Serial.

> GPIO 34 ist nur ein sinnvoller Standard für klassische ESP32-DevKit-Boards. Vor dem Flashen muss `include/AppConfig.h` an dein tatsächliches Board und deine Verdrahtung angepasst werden.

## Lokale Libraries

Das Firmwareprojekt verwendet in `platformio.ini` lokale Abhängigkeiten mit `symlink://../mea-core` usw. PlatformIO verlinkt damit die bestehenden Repository-Verzeichnisse, statt Kopien anzulegen. Änderungen an einer Library sind dadurch unmittelbar im Firmware-Build sichtbar.

## Git-Remotes einrichten

### Gitea/Forgejo per API

Erstelle zuerst in Gitea unter `Einstellungen -> Anwendungen` ein persönliches Zugriffstoken mit Schreibrecht auf Repositories. Den Token gibst du nur im Terminal ein, nicht in eine Datei und nicht in die Git-URL.

In zsh:

```zsh
read -s "GITEA_TOKEN?Gitea-Token: "
echo
export GITEA_TOKEN
```

Dann alle Repositories auf deinem Server anlegen und die lokalen `origin`-Remotes setzen:

```bash
./scripts/create-gitea-repositories.sh \
  'http://192.168.178.99:3000' \
  'Theo' \
  user \
  private
```

Prüfen und pushen:

```bash
git -C repositories/mea-core remote -v
./scripts/push-all.sh
unset GITEA_TOKEN
```

Beim `git push` über HTTP verwendest du `Theo` als Benutzernamen und dein Gitea-Token als Passwort.

### Einfacher SSH-Git-Server

Bare Repositories auf einem per SSH erreichbaren Git-Server erstellen:

```bash
./scripts/create-bare-remotes.sh git@gitserver.local /srv/git/embedded
```

Remotes in allen lokalen Repositories setzen:

```bash
./scripts/configure-remotes.sh 'git@gitserver.local:/srv/git/embedded'
```

Ersten Stand pushen:

```bash
./scripts/push-all.sh
```

Wenn du die Repositories in Gitea/GitLab manuell anlegst, funktioniert `configure-remotes.sh` ebenso, zum Beispiel:

```bash
./scripts/configure-remotes.sh 'http://192.168.178.99:3000/Theo'
```

## Wichtige Dokumente

- `docs/00-VERWENDUNG-UND-KONFIGURATION.md`
- `docs/01-ARCH-LINUX-VSCODE.md`
- `docs/02-ARCHITEKTUR.md`
- `docs/03-GIT-UND-VERSIONIERUNG.md`
- `docs/04-TESTS-UND-QUALITAET.md`
- `docs/05-NEUE-LIBRARY-ANLEGEN.md`
- `docs/07-CODE-TOUR-FUER-TEAMS.md`

## Designregeln

1. Hardware-Libraries kennen weder Manager noch Zustandsmaschine.
2. Die Core-Library kennt keine konkrete Hardware.
3. Konfigurationen werden von außen übergeben.
4. Kommunikation und Verarbeitung arbeiten mit standardisierten `Measurement`-Paketen.
5. Manager besitzen Komponenten nicht, sondern registrieren Referenzen auf statisch erzeugte Objekte.
6. Kein `new`, kein `delete`, keine versteckten globalen Singletons und keine blockierenden `delay()`-Aufrufe in den Libraries.
7. Die Anwendung in `main.cpp` ist der Composition Root: Dort werden konkrete Bausteine erstellt und verbunden.

## Lizenz

Die Beispiel-Repositories stehen unter der MIT-Lizenz.
