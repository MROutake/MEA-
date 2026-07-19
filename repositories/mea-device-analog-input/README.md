# MEA Analog Input Device

`mea-device-analog-input` liefert eine nicht blockierende ADC-Quelle. Die
Library trennt Messwertlogik (`AnalogInputSensor`) vom Hardwarezugriff
(`IAnalogReader`/`ArduinoAnalogReader`).

Zielstand nach Umbauplan:
[../../docs/08-UMBAUPLAN-MODULARE-EINHEIT.md](../../docs/08-UMBAUPLAN-MODULARE-EINHEIT.md).

## Rolle im Zielsystem

```mermaid
flowchart LR
    ADC[ADC Pin]
    Reader[IAnalogReader]
    Sensor[AnalogInputSensor]
    Runtime[MeasurementNode]
    Processing[mea-processing]
    Sink[Serial/ESP-NOW Sink]

    ADC --> Reader --> Sensor --> Runtime --> Processing --> Sink
```

## Zielnutzung mit Runtime

```cpp
mea::ArduinoAnalogReader analogReader(board::kAdcMaximumRaw);
mea::AnalogInputSensor analogSensor(analogReader, {
    ids::AnalogInput1,
    board::kAnalogInputPin,
    config::kSensorSampleIntervalMs,
    config::kSamplesPerMeasurement,
    config::kMaxSamplesPerUpdate,
    mea::MeasurementKind::RawAnalog,
    mea::Unit::RawCount,
});

node.addPipeline(ids::SoilVoltagePipeline, analogSensor)
    .through(rawToVoltage, voltageClamp)
    .into(serialSink);
```

Die Umrechnung nach Volt gehoert nicht in diese Library, sondern in
`mea-processing`.

## Laufzeitverhalten

1. `begin()` initialisiert den Pin ueber `IAnalogReader`.
2. `update(nowMs)` nimmt nur begrenzt viele Rohsamples.
3. Nach `samplesPerMeasurement` Rohsamples wird ein `Measurement` erzeugt.
4. `available()`/`read()` liefern fertige Werte an die Pipeline.
5. Bei voller Queue wird der neue fertige Wert verworfen und gezaehlt.

## Zentrale Dateien

| Datei | Verantwortung |
|---|---|
| [src/MeaAnalogInput.h](src/MeaAnalogInput.h) | Sammel-Header |
| [src/mea/device/IAnalogReader.h](src/mea/device/IAnalogReader.h) | HAL-Interface |
| [src/mea/device/ArduinoAnalogReader.h](src/mea/device/ArduinoAnalogReader.h) | Arduino-Adapter |
| [src/mea/device/AnalogInputSensor.h](src/mea/device/AnalogInputSensor.h) | MEA-Source |
| [src/mea/device/testing/FakeAnalogReader.h](src/mea/device/testing/FakeAnalogReader.h) | Fake fuer native Tests |

## Abhaengigkeiten

| Dependency | Warum |
|---|---|
| [../mea-core](../mea-core) | `IMeasurementSource`, `Measurement`, `Status`, `RingBuffer` |

## Regeln fuer neue analoge Profile

1. Pin und ADC-Maximalwert im Composition Root konfigurieren.
2. Rohwert als `RawAnalog/RawCount` ausgeben.
3. Kalibrierung und Skalierung ueber Prozessoren modellieren.
4. Keine Board-Konstanten in diese Library aufnehmen.

## Testen

```bash
pio test -e native
```
