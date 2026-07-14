# Netzwerk-Integration: neue Sensoren und ICs anbinden

Kurzanleitung, wie neue Messquellen (Sensoren) und Ausgänge (ICs) über den
MEA-Netzwerk-Stack (`mea-protocol`, `mea-network-core`, `mea-network-ws500`)
telemetriert werden – ohne den bestehenden Datenfluss zu ändern.

## Überblick der Schichten

```text
Sensor/IC ──> Pipeline ──> IMeasurementSink
                              ├─ bestehender Sink (z. B. 74HC595, Serial/CSV)
                              └─ NetworkMeasurementSink
                                     └─ ProtocolBridge ── BinaryMessageCodec (mea-protocol)
                                              └─ NetworkSession ── INetworkTransport
                                                        └─ Ws500Transport (mea-network-ws500)
```

Harte Regeln: `mea-protocol` kennt keinen Transport, `mea-network-ws500` kennt
kein Protokoll. Die Firmware kennt nur Interfaces.

## Neuen Sensor telemetrieren (Regelfall: keine Protokolländerung)

Jede `IMeasurementSource` fließt automatisch als `Measurement`-Nachricht ins
Netz, sobald ihre Messwerte einen `NetworkMeasurementSink` erreichen.

1. Sensor-Library wie gewohnt anlegen (siehe
   [05-NEUE-LIBRARY-ANLEGEN.md](05-NEUE-LIBRARY-ANLEGEN.md)).
2. Im Composition Root registrieren und die Pipeline-`sinkIds` um den
   `NetworkMeasurementSink` ergänzen (der Sink wird ein Sink wie jeder andere).
3. Fertig – `MeasurementKind`/`Unit`/`quality` werden mitübertragen. Neue
   Mess­arten/Einheiten nur bei Bedarf in `mea-core` (`Measurement.h`) ergänzen;
   der Codec überträgt sie als numerische Werte ohne Änderung.

```cpp
constexpr mea::ComponentId kSinkIds[] = { ids::ExistingOutput, ids::NetworkOut };
// ... NetworkMeasurementSink registrieren, Pipeline mit beiden Sinks bauen.
```

## Neuen IC/Ausgangszustand melden (`OutputState`)

Für IC-Zustände (z. B. 74HC595-Kanäle) gibt es die `OutputState`-Nachricht.
Sie wird nicht über die Messwert-Pipeline, sondern direkt über die
`ProtocolBridge` publiziert (z. B. nach `commit()` des Treibers):

```cpp
mea::protocol::OutputStatePayload state{};
state.componentId = driverId;
state.channelCount = 16;
state.byteCount    = 2;
state.state[0] = bank0; state.state[1] = bank1;
state.appliedAtMs = nowMs;
bridge.publish(mea::protocol::makeOutputStateEnvelope(driverId, 0, seq++, nowMs, state));
```

Analog für `StateTransition` (Pipeline-/Orchestrator-Übergänge), `ErrorEvent`
(aus `mea::Status`) und `Heartbeat` (Lebenszeichen). Builder je Typ liegen in
`mea/protocol/MessageEnvelope.h`.

## Neuen Nachrichtentyp einführen (selten)

1. `MessageKind` in `mea/protocol/MessageKind.h` ergänzen und dort
   `payloadLengthFor()`/`messageKindName()` erweitern.
2. Payload-Struct in `Payloads.h` definieren (trivial kopierbar).
3. Union-Feld + Builder in `MessageEnvelope.h` ergänzen.
4. Encode/Decode-Zweig in `BinaryMessageCodec.cpp` hinzufügen.
5. Golden-Frame- und Roundtrip-Test in `mea-protocol` ergänzen.
6. `kProtocolVersion` nur bei inkompatibler Änderung des Rahmenformats erhöhen;
   neue optionale Payload-Felder werden am Ende angehängt (Vorwärts-Skip über
   `payloadLength`).

## Transport wechseln

`Ws500Transport` gegen eine andere `INetworkTransport`-Implementierung tauschen –
`mea-protocol` und `mea-network-core` bleiben unverändert. Für reale WS500-
Hardware `ArduinoWs500Client<EthernetClient>` verwenden (siehe
`mea-network-ws500/examples/demo-pipeline-integration`).

## Checkliste

- Netzwerk-Sink als zusätzlicher Pipeline-Sink registriert (bestehende Sinks
  unangetastet).
- `NetworkSession.begin()/connect()` und `bridge.begin()` im Composition Root
  aufgerufen; `bridge.update()` läuft je Zyklus (z. B. via `sink.update()`).
- IDs in der zentralen ID-Datei vergeben (`InvalidComponentId` verboten).
- Native Tests grün (`pio test -e native` je Repo).
