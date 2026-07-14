# MEA Protocol

Transport-unabhängiges, binäres Netzwerkprotokoll für die MEA-Plattform.
Serialisiert Messwerte, Ausgangszustände, Zustandsübergänge, Fehler,
Heartbeats und optional Kommandos in kompakte, selbstsynchronisierende Frames.

Vollständige Spezifikation: [docs/PROTOCOL-SPEC-v0.1.md](docs/PROTOCOL-SPEC-v0.1.md).

## Abhängigkeiten

- `mea-core` (nur ID-, Zeit-, Status- und Measurement-Typen).

Keine Transport-Abhängigkeit (WS500, Serial, TCP). Kein dynamischer Speicher.

## Zentrale Dateien

| Datei | Inhalt |
|-------|--------|
| `mea/protocol/MessageKind.h` | Nachrichtenarten + erwartete Payload-Längen |
| `mea/protocol/MessageHeader.h` | Header + Wire-Konstanten (`kFrameMagic`, `kMaxFrameSize`) |
| `mea/protocol/Payloads.h` | Payload-Strukturen (POD) |
| `mea/protocol/MessageEnvelope.h` | Header + Payload-Union + Builder |
| `mea/protocol/IMessageCodec.h` | `IMessageEncoder` / `IMessageDecoder` |
| `mea/protocol/BinaryMessageCodec.h/.cpp` | Little-Endian-Codec mit CRC-16 |
| `mea/protocol/MessageValidator.h` | Struktur-/Versionsprüfung |
| `mea/protocol/ComponentRegistry.h` | `{ComponentId, erlaubte Kinds}`-Registry |

## Beispiel

```cpp
#include <MeaProtocol.h>
using namespace mea::protocol;

BinaryMessageCodec codec;

// Senden: Measurement -> Frame
mea::Measurement m{ /* ... */ };
MessageEnvelope out = makeMeasurementEnvelope(/*origin*/ 610, /*target*/ 0,
                                              /*seq*/ 1, /*ts*/ millis, m);
std::uint8_t frame[kMaxFrameSize];
std::size_t written = 0;
codec.encode(out, frame, sizeof(frame), written);

// Empfangen: Frame -> Envelope
MessageEnvelope in;
std::size_t consumed = 0;
if (codec.decode(frame, written, in, consumed).ok()) {
    // in.header.kind diskriminiert in.payload.*
}
```

## Tests

```bash
pio test -e native
```

Deckt Roundtrips aller Nachrichtentypen, Golden Frames (byte-genau) und
Fehlerfälle (zu kleiner Puffer, unvollständig, falsches Magic/Version, CRC-Fehler,
inkonsistente Länge) ab.
