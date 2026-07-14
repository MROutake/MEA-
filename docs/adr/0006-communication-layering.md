# ADR 0006 – Schichtung der Kommunikation

Status: akzeptiert · Datum: 2026-07-13

## Kontext

Der bisherige `SerialCsvSink` vereinte Hardwaretransport (`Stream`), Serialisierung und
Sink-Logik; `Stream::print` kann blockieren, es gab keine Queue und kein Backpressure.

## Entscheidung

1. **Drei Schichten** in `mea-communication`:
   - **Transport:** `IByteTransport` (nicht blockierend, partielle Writes über
     `written`-Out-Parameter, `writable()`/`readable()` zur Kapazitätsabfrage).
     Implementierungen: `ArduinoStreamTransport` (einzige Arduino-abhängige Klasse,
     nutzt `availableForWrite()`), `FakeByteTransport` (native Tests, injizierbare Fehler
     und Schreiblimits).
   - **Kodierung:** `IMeasurementEncoder`; `CsvMeasurementEncoder` serialisiert in einen
     Aufruferpuffer via `snprintf` mit strikter Kapazitätsprüfung.
   - **Sink:** `BufferedMeasurementSink<QueueCapacity, FrameSize>` implementiert
     `IMeasurementSink`, hält eine feste Messwert-Queue (`RingBuffer`), kodiert je
     Messwert einen Frame und schreibt ihn in `update()` nicht blockierend (partielle
     Writes werden fortgesetzt).
2. **CSV-Format (Version 1):**

   ```text
   1;source_id;kind;unit;value;sampled_at_ms;sequence;quality\n
   ```

   Feldtrenner `;`, Dezimaltrenner `.`, `kind/unit/quality` numerisch, Zeilenende `\n`.
   Das Versionsfeld ist das erste Feld jeder Zeile.
3. **Backpressure:** Volle Queue → `submit()` gibt `WouldBlock` zurück; der Messwert wird
   nicht übernommen und `rejectedCount` erhöht (kein stiller Verlust – der Aufrufer sieht
   den Status, der Zähler macht ihn diagnostizierbar).
4. **Fehler:** Encoder-/Transportfehler verwerfen den betroffenen Frame, erhöhen
   `encodeErrorCount`/`transportErrorCount` und melden den Status des `update()`-Aufrufs.
5. **Migration:** `SerialCsvSink` wird entfernt. Ersatz:
   `CsvMeasurementEncoder` + `ArduinoStreamTransport` + `BufferedMeasurementSink`
   (Verdrahtung im Composition Root). Der CSV-Header entfällt; das Versionsfeld ersetzt ihn.
6. **Eingehende Kommandos (Vorbereitung):** `LineCommandDecoder` implementiert
   `ICommandSource`, liest zeilenweise `target;type;argument` aus einem `IByteTransport`
   (fester Zeilenpuffer, kein JSON, keine Allokation). Noch nicht in der Demo verdrahtet.

## Konsequenzen

- Encoder und Sink sind vollständig nativ testbar; Arduino-Abhängigkeit ist auf eine
  Klasse begrenzt.
- RAM je Sink: `QueueCapacity · sizeof(Measurement) + FrameSize` (statisch ablesbar).
