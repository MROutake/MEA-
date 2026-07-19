# MEA Processing

`mea-processing` enthaelt hardwarefreie Messwertverarbeitung. Die Library kennt
nur `mea::Measurement`, `mea::Status` und `mea::IMeasurementProcessor` aus
`mea-core`.

Zielstand nach Umbauplan:
[../../docs/08-UMBAUPLAN-MODULARE-EINHEIT.md](../../docs/08-UMBAUPLAN-MODULARE-EINHEIT.md).

## Rolle im Zielsystem

```mermaid
flowchart LR
    Source[Source]
    P1[LinearProcessor]
    P2[Clamp/Range/MovingAverage]
    Sink[Sink]

    Source --> P1 --> P2 --> Sink
```

Processing ist bewusst fachlich, aber nicht hardwarebezogen. ADC-Rohwerte,
Temperaturwerte oder Druckwerte koennen verarbeitet werden, ohne dass diese
Library weiss, von welchem Sensor sie kommen.

## Prozessoren

| Prozessor | Zweck |
|---|---|
| `PassThroughProcessor` | unveraenderter Durchlauf fuer Tests oder Platzhalter |
| `LinearProcessor` | lineare Umrechnung, z. B. Rohwert -> Volt oder Pa -> hPa |
| `ClampProcessor` | Wert begrenzen und `QualityFlag::OutOfRange` setzen |
| `RangeValidationProcessor` | Wertebereich markieren, Wert unveraendert lassen |
| `MovingAverageProcessor<N>` | gleitender Mittelwert ohne Heap |

## Zielnutzung mit Runtime

```cpp
mea::LinearProcessor rawToVoltage({
    ids::RawToVoltage,
    3.3F / 4095.0F,
    0.0F,
    mea::MeasurementKind::RawAnalog,
    mea::Unit::RawCount,
    mea::MeasurementKind::Voltage,
    mea::Unit::Volt,
});

mea::ClampProcessor voltageClamp({
    ids::VoltageClamp,
    0.0F,
    3.3F,
    mea::MeasurementKind::Voltage,
    mea::Unit::Volt,
});

node.addPipeline(ids::SoilVoltagePipeline, analogSensor)
    .through(rawToVoltage, voltageClamp)
    .into(serialSink);
```

## Regeln fuer neue Prozessoren

1. `IMeasurementProcessor` implementieren.
2. Eingabe nie veraendern, Ergebnis in `output` schreiben.
3. `accepts(kind, unit)` konsequent nutzen.
4. Operationserfolg ueber `Status` melden.
5. Fachliche Einschraenkungen ueber `Measurement::quality` markieren.
6. Keine Arduino- oder Board-Abhaengigkeit einfuehren.

## Zentrale Dateien

| Datei | Verantwortung |
|---|---|
| [src/MeaProcessing.h](src/MeaProcessing.h) | Sammel-Header |
| [src/mea/processing/PassThroughProcessor.h](src/mea/processing/PassThroughProcessor.h) | neutraler Prozessor |
| [src/mea/processing/LinearProcessor.h](src/mea/processing/LinearProcessor.h) | lineare Umrechnung |
| [src/mea/processing/ClampProcessor.h](src/mea/processing/ClampProcessor.h) | Begrenzung |
| [src/mea/processing/RangeValidationProcessor.h](src/mea/processing/RangeValidationProcessor.h) | Validierung |
| [src/mea/processing/MovingAverageProcessor.h](src/mea/processing/MovingAverageProcessor.h) | Filter |

## Abhaengigkeiten

| Dependency | Warum |
|---|---|
| [../mea-core](../mea-core) | `Measurement`, `Status`, Processor-Interface |

## Testen

```bash
pio test -e native
```
