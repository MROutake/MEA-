# Laufzeitverhalten

## Datenfluss

```text
ESP32 ADC GPIO 34
    ↓  (IAnalogReader: ArduinoAnalogReader)
AnalogInputSensor            ← nicht blockierendes Oversampling (8 Samples, max. 2/Update)
    ↓
LinearProcessor (RawToVoltage)   ← Rohwert → Volt (Gain aus BoardConfig)
    ↓
ClampProcessor (VoltageClamp)    ← begrenzt auf 0 … 3,3 V, setzt OutOfRange-Flag
    ↓
BufferedMeasurementSink (SerialOutput)  ← Queue (8), Frame (96 B)
    ↓
CsvMeasurementEncoder        ← "1;100;2;2;1.650;12345;42;0\n"
    ↓
ArduinoStreamTransport → Serial (115200 Baud)
```

Koordiniert wird der Fluss von der `MeasurementPipelineMachine`
(ID 400, Zyklus 1000 ms, Akquise-Timeout 2000 ms, Publish-Timeout 500 ms,
Retry: 3 × mit 250 ms Abstand).

## Ausgabeformat (CSV Version 1)

```text
version;source_id;kind;unit;value;sampled_at_ms;sequence;quality
```

Beispiel: `1;100;2;2;1.650;12345;42;0` = Quelle 100, Voltage (2) in Volt (2),
1,650 V, Zeitstempel 12345 ms, Sequenz 42, Qualität uneingeschränkt.

Fehlermeldungen der Anwendung beginnen mit `[mea]` und enthalten Statusname,
verursachende Komponenten-ID (`origin`) und Detailcode.

## Hauptschleife

`loop()` ruft ausschließlich `application.update(millis())` auf; dort werden
nacheinander (alle nicht blockierend) ausgeführt:

1. `sources.updateAll(now)` – Sensor sammelt Samples / legt Messwerte ab.
2. `transport.update(now)` – Transport-Housekeeping.
3. `sinks.updateAll(now)` – Sink schreibt gepufferte Frames (partiell) nach.
4. `pipeline.update(now)` – Zustandsmaschine koordiniert den Zyklus.

Kein `delay()`, keine Endlosschleifen; die Arbeit pro `loop()`-Durchlauf ist
durch Konfigurationskonstanten begrenzt.

## RAM-relevante feste Puffer (Demo)

| Puffer | Größe |
|---|---|
| Sensor-Messwertqueue | 4 × 20 B = 80 B |
| Sink-Queue | 8 × 20 B = 160 B |
| Sink-Framepuffer | 96 B |
| Manager-Slots (3 × 4 Komponenten + Health) | ≈ 3 × (4 × 8 B + 4 × 20 B) ≈ 340 B |
| Pipeline-Maschine (Caches, 2 Messwerte) | ≈ 150 B |

## Build & Flash

```bash
pio run -e esp32dev                # kompilieren
pio run -e esp32dev -t upload      # flashen
pio device monitor                 # CSV-Ausgabe beobachten (115200 Baud)
pio test -e native                 # kompletter Pipeline-Test ohne Hardware
pio test -e esp32dev_test          # Embedded-Smoke-Test (Board erforderlich)
```
