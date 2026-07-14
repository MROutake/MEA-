# MEA Network Protocol – Spezifikation v0.1

Status: Entwurf · Datum: 2026-07-14 · Gilt für `mea-protocol` 0.1.0

## 1. Ziel und Abgrenzung

Ein transport-unabhängiges, binäres Rahmenformat, um MEA-Daten (Messwerte,
Ausgangszustände, Zustandsübergänge, Fehler, Heartbeats, optional Kommandos)
zwischen Geräten und einer Gegenstelle auszutauschen.

Grundsätze (analog ADR 0001–0006):

- **Keine dynamische Allokation.** Feste Puffer, Compile-Time-Kapazitäten.
- **Transport-neutral.** Das Protokoll kennt keine WS500-/Serial-/TCP-Interna.
  Es serialisiert nur in einen Aufruferpuffer und deserialisiert aus einem
  Aufruferpuffer (wie `IMeasurementEncoder`, ADR 0006).
- **Explizite Byte-Reihenfolge (Little-Endian).** Keine `struct`-Packung über
  die Leitung, damit Golden Frames plattformunabhängig reproduzierbar sind und
  `-Wconversion`/Alignment keine Rolle spielt.
- **Selbstsynchronisierend.** Jeder Frame beginnt mit einem 2-Byte-Magic und
  endet mit CRC-16, sodass ein Byte-Strom-Transport Framegrenzen wiederfinden
  kann.

Das Protokoll baut auf `mea-core` auf (nur ID-, Zeit-, Status- und
Measurement-Typen). Es hängt **nicht** von `mea-communication`,
`mea-network-core` oder `mea-network-ws500` ab.

## 2. Rahmenformat (Wire Format)

Ein Frame ist eine zusammenhängende Byte-Folge:

```text
+---------+------------------------+----------------------+---------+
| MAGIC   |   HEADER (16 Byte)     |  PAYLOAD (0..N Byte) | CRC16   |
| 2 Byte  |   (inkl. MAGIC = 18)   |                      | 2 Byte  |
+---------+------------------------+----------------------+---------+
```

Alle Mehrbyte-Felder sind **Little-Endian**.

### 2.1 Header (18 Byte inkl. Magic)

| Offset | Feld              | Typ    | Beschreibung                                   |
|-------:|-------------------|--------|------------------------------------------------|
| 0      | `magic`           | u16    | `0x414D` (ASCII `"MA"`, LE: `0x4D 0x41`)       |
| 2      | `protocolVersion` | u8     | Major-Version des Rahmenformats (aktuell `1`)  |
| 3      | `kind`            | u8     | `MessageKind` (siehe §3)                        |
| 4      | `componentId`     | u16    | Ursprungskomponente (`mea::ComponentId`, ≠ 0)  |
| 6      | `targetId`        | u16    | Zielkomponente (`0` = Broadcast)               |
| 8      | `sequence`        | u32    | Sequenznummer je Sender (Überlauf erlaubt)     |
| 12     | `timestampMs`     | u32    | Sendezeit (`mea::TimestampMs`, `millis()`)     |
| 16     | `payloadLength`   | u16    | Länge des Payloads in Byte (0..`kMaxPayload`)  |

`kMaxPayload = 255` (v0.1). Größere Payloads sind reserviert.

### 2.2 CRC

`crc16` = CRC-16/CCITT-FALSE (Polynom `0x1021`, Init `0xFFFF`, kein
Reflect, kein XorOut) über **Header inklusive Magic + Payload** (also alle
Bytes vor dem CRC). Fehler im Magic werden dadurch ebenfalls erkannt.

### 2.3 Gesamtlänge

`frameLength = 18 + payloadLength + 2 = 20 + payloadLength`.
Ein leerer Payload (`payloadLength = 0`) ergibt einen 20-Byte-Frame.

## 3. Nachrichtentypen (`MessageKind`)

| Wert | Kind              | Payload-Struct       | Payload-Länge |
|-----:|-------------------|----------------------|--------------:|
| 0    | `Unknown`         | –                    | –             |
| 1    | `Measurement`     | Measurement (§4.1)   | 18            |
| 2    | `OutputState`     | OutputState (§4.2)   | 16            |
| 3    | `StateTransition` | StateTransition (§4.3)| 12           |
| 4    | `ErrorEvent`      | ErrorEvent (§4.4)    | 12            |
| 5    | `Heartbeat`       | Heartbeat (§4.5)     | 14            |
| 6    | `Command`         | Command (§4.6)       | 14            |

Fünf Kernnachrichten (`Measurement`, `OutputState`, `StateTransition`,
`ErrorEvent`, `Heartbeat`) sind verpflichtend; `Command` ist optional
(eingehende Steuerung).

## 4. Payload-Layouts (alle Little-Endian)

### 4.1 Measurement (18 Byte) — Abbild von `mea::Measurement`

| Off | Feld         | Typ | Bezug                          |
|----:|--------------|-----|--------------------------------|
| 0   | `sourceId`   | u16 | `Measurement::sourceId`        |
| 2   | `kind`       | u8  | `MeasurementKind`              |
| 3   | `unit`       | u8  | `Unit`                         |
| 4   | `value`      | f32 | IEEE-754, LE                   |
| 8   | `sampledAtMs`| u32 | `Measurement::sampledAtMs`     |
| 12  | `sequence`   | u32 | `Measurement::sequence`        |
| 16  | `quality`    | u16 | `QualityFlag`                  |

### 4.2 OutputState (16 Byte) — IC-/Treiber-Ausgangszustand

| Off | Feld         | Typ      | Beschreibung                          |
|----:|--------------|----------|---------------------------------------|
| 0   | `componentId`| u16      | Treiber-/IC-ID (z. B. 74HC595-Sink)   |
| 2   | `channelCount`| u8      | Anzahl gültiger Kanäle                 |
| 3   | `byteCount`  | u8       | Anzahl gültiger Zustands-Bytes (0..8) |
| 4   | `state[8]`   | u8[8]    | Bitmaske je Byte (Kanal = Bit)        |
| 12  | `appliedAtMs`| u32      | Zeitpunkt des `commit()`              |

### 4.3 StateTransition (12 Byte) — Pipeline-/Orchestrator-Übergang

| Off | Feld         | Typ | Beschreibung                                |
|----:|--------------|-----|---------------------------------------------|
| 0   | `componentId`| u16 | Pipeline-/Orchestrator-ID                   |
| 2   | `fromState`  | u8  | Vorher (`PipelineState`/`Phase`)            |
| 3   | `toState`    | u8  | Nachher                                     |
| 4   | `reason`     | u16 | `StatusCode` oder Phasen-Detail             |
| 6   | `flags`      | u16 | reserviert (0)                              |
| 8   | `atMs`       | u32 | Zeitpunkt                                   |

### 4.4 ErrorEvent (12 Byte) — Abbild von `mea::Status`

| Off | Feld         | Typ | Bezug                              |
|----:|--------------|-----|------------------------------------|
| 0   | `componentId`| u16 | `Status::origin`                   |
| 2   | `code`       | u8  | `StatusCode`                       |
| 3   | `severity`   | u8  | 0 = info, 1 = warn, 2 = error      |
| 4   | `detail`     | u16 | `Status::detail`                   |
| 6   | `flags`      | u16 | reserviert (0)                     |
| 8   | `atMs`       | u32 | Zeitpunkt                          |

### 4.5 Heartbeat (14 Byte) — Lebenszeichen

| Off | Feld         | Typ | Beschreibung                       |
|----:|--------------|-----|------------------------------------|
| 0   | `componentId`| u16 | Absender (Gerät/Node)              |
| 2   | `uptimeMs`   | u32 | Betriebszeit                       |
| 6   | `sequence`   | u32 | Heartbeat-Zähler                   |
| 10  | `flags`      | u16 | Health-Flags (frei belegbar)       |
| 12  | `state`      | u8  | grober Node-Zustand                |
| 13  | `reserved`   | u8  | 0                                  |

### 4.6 Command (14 Byte) — Abbild von `mea::Command` (optional)

| Off | Feld         | Typ | Bezug                              |
|----:|--------------|-----|------------------------------------|
| 0   | `sourceId`   | u16 | `Command::sourceId`                |
| 2   | `targetId`   | u16 | `Command::targetId`                |
| 4   | `type`       | u16 | `CommandType`                      |
| 6   | `argument`   | u32 | `Command::argument`                |
| 10  | `atMs`       | u32 | `Command::receivedAtMs`            |

## 5. Encoder/Decoder-Interfaces

Analog ADR 0006, ohne Allokation:

```cpp
class IMessageEncoder {
  virtual Status encode(const MessageEnvelope& in, std::uint8_t* out,
                        std::size_t capacity, std::size_t& written) const noexcept = 0;
};
class IMessageDecoder {
  // consumed meldet die Byte-Anzahl des erkannten Frames (fürs Nachrücken).
  virtual Status decode(const std::uint8_t* in, std::size_t size,
                        MessageEnvelope& out, std::size_t& consumed) const noexcept = 0;
};
```

`BinaryMessageCodec` implementiert beide. Fehlercodes:

- `InvalidArgument` – Nullzeiger, `kind == Unknown`, ungültige Payload-Länge.
- `CapacityExceeded` – Ausgabepuffer zu klein (`detail` = benötigte Länge).
- `NoData` – Eingabe kürzer als ein vollständiger Frame.
- `ProtocolError` – Magic/Version/Kind ungültig, Payload-Länge passt nicht.
- `ChecksumError` – CRC stimmt nicht.

## 6. Validierung und Registry

- `MessageValidator`: prüft `magic`, `protocolVersion`, gültigen `kind`,
  erwartete `payloadLength` je `kind` und `componentId != 0`.
- `ComponentRegistry<N>`: nicht besitzende, feste Registry aus
  `{componentId, erlaubte MessageKinds (Bitmaske)}`. Beantwortet, ob eine
  Komponente bekannt ist und einen `kind` senden/empfangen darf. Nutzung ist
  optional (Policy des Composition Root).

## 7. Versionierung

`protocolVersion` ist die **Major**-Version des Rahmenformats. Ein Decoder
lehnt fremde Major-Versionen mit `ProtocolError` ab. Neue optionale Felder
werden ausschließlich am Payload-Ende ergänzt; `payloadLength` erlaubt
Vorwärts-Skip. Die Library-Version (`Version.h`/`library.json`) ist davon
unabhängig (SemVer der Implementierung).

## 8. Golden Frames (Referenz)

Heartbeat, `componentId=0x0203 (515)`, `uptimeMs=0x00000100`,
`sequence=0x00000007`, `flags=0x0000`, `state=1`, `sequence(header)=7`,
`timestampMs=0x000004D2 (1234)`, `targetId=0`:

```text
4D 41 01 05 03 02 00 00 07 00 00 00 D2 04 00 00 0E 00   ; Header(18)
03 02 00 01 00 00 07 00 00 00 00 00 01 00             ; Payload(14)
<crc-lo> <crc-hi>                                      ; CRC16
```

Die exakten CRC-Bytes und weitere Golden Frames sind als Testvektoren in
`test/native/test_protocol/main.cpp` hinterlegt (Roundtrip + Byte-Vergleich).

## 9. Offene Punkte (v0.2)

- Fragmentierung großer Payloads (`payloadLength > 255`).
- Authentisierung/Integrität über CRC hinaus (z. B. MAC).
- Batch-Frames (mehrere Payloads je Frame).
- Registry-gestützte Richtliniendurchsetzung im `ProtocolBridge`.
