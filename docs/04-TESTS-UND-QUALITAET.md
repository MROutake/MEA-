# Tests und Qualität

## Testarten

1. **Native Unit Tests:** Logik läuft auf dem Arch-Linux-PC.
2. **Embedded Unit Tests:** Test läuft auf dem Mikrocontroller.
3. **Smoke Build:** Prüft, ob alle Libraries zusammen für das Zielboard kompilieren.
4. **Hardware-in-the-Loop:** Prüft echte Sensoren und Schnittstellen.

## Enthaltener Native-Test

```bash
cd repositories/mea-demo-firmware
pio test -e native
```

Der Test verwendet einen Fake-Sensor und einen Fake-Sink. Getestet wird die echte Verarbeitung, die echten Manager und die echte Zustandsmaschine, ohne Arduino- oder ESP32-Abhängigkeit.

## Firmware bauen

```bash
pio run -e esp32dev
```

Dies ist gleichzeitig der Integrations-/Smoke-Test für `mea-device-analog-input` und `mea-communication`.

## Embedded-Test bauen und ausführen

Mit angeschlossenem ESP32:

```bash
pio test -e esp32dev_test
```

## Statische Analyse

```bash
pio check -e esp32dev
```

Zusätzlich kann eine Compile-Datenbank erzeugt werden:

```bash
pio run -e esp32dev -t compiledb
```

## Qualitätsregeln

- Compilerwarnungen nicht ignorieren.
- Öffentliche APIs dokumentieren.
- Hardwarezugriffe hinter Interfaces kapseln.
- Fachlogik möglichst ohne `Arduino.h` implementieren.
- Zeitabhängigkeiten über übergebene Zeitwerte testen.
- Fehler als `Status` weiterreichen, nicht still verschlucken.
- Operationsstatus (`Status`) und Datenqualität (`Measurement::quality`) getrennt behandeln.
- Tests vor Tags und Releases ausführen.

## Alle Prüfungen

Vom Workspace-Root:

```bash
./scripts/test-all.sh
./scripts/build-demo.sh
```
