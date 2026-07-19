# ADR 0003 – Messwertformat

Status: akzeptiert · Datum: 2026-07-13

## Kontext

Im Scaffold trug `Measurement` ein `Status`-Feld und diente damit gleichzeitig als
Kontrollfluss für Fehler. Operationsstatus und Datenqualität sind aber verschiedene Dinge:
Ein `read()` kann erfolgreich sein (`Status::Ok`) und trotzdem einen qualitativ
eingeschränkten Wert liefern (z. B. `Stale` oder `OutOfRange`).

## Entscheidung

1. `Measurement` enthält **keinen Operationsstatus**, sondern eine Qualitäts-Bitmaske:

   ```cpp
   struct Measurement {
       ComponentId     sourceId{InvalidComponentId};
       MeasurementKind kind{MeasurementKind::Unknown};
       Unit            unit{Unit::None};
       float           value{0.0F};
       TimestampMs     sampledAtMs{0};
       SequenceNumber  sequence{0};
       QualityFlag     quality{QualityFlag::None};
   };
   ```

2. `MeasurementKind` und `Unit` wie im Anforderungsdokument (inkl. `Resistance`,
   `Frequency`, `MilliVolt`, `MilliAmpere`, `Ohm`, `Hertz`).
3. `QualityFlag` ist eine Bitmaske (`Stale`, `OutOfRange`, `Estimated`, `SensorFault`,
   `CommunicationFault`) mit `constexpr`-Bitoperatoren und `hasFlag()`.
4. `sequence` wird je Quelle monoton erhöht (Überlauf von `std::uint32_t` erlaubt);
   Lücken zeigen verlorene Messwerte an.
5. `sampledAtMs` ist der Zeitpunkt des **Abschlusses** der Messung (bei Oversampling: der
   Zeitpunkt, zu dem das letzte Sample genommen wurde).
6. Validierung über freie Funktionen: `isValid(measurement)` prüft gültige `sourceId` und
   endlichen `value` (`std::isfinite`). Compile-Time-Zusicherungen: trivial kopierbar,
   `sizeof(Measurement) <= 20`.
7. **Ausdrücklich:** Ein erfolgreicher Funktionsstatus (`Status::ok()`) und die
   Messwertqualität (`Measurement::quality`) sind zwei verschiedene Aussagen. Prozessoren
   und Sinks dürfen Werte mit gesetzten Qualitätsflags weiterreichen; nur die Anwendung
   entscheidet, wie sie darauf reagiert.

## Konsequenzen

- Das CSV-Format (ADR 0006) transportiert `quality` als numerisches Feld.
- Prozessoren wie `RangeValidationProcessor` melden Bereichsverletzungen über
  Qualitätsflags statt über Fehlerstatus.

## Erweiterungen

- 2026-07-19: `MeasurementKind::SoilMoisture` und `Unit::Hectopascal` ergänzt
  (Sensor-Node: kapazitiver Bodenfeuchtesensor, BMP280-Luftdruck in hPa).
  Regel: Beide Enums werden ausschließlich durch **Anhängen** erweitert, damit
  die numerischen Codes im CSV-Protokoll stabil bleiben; `kFormatVersion`
  bleibt dadurch unverändert.
