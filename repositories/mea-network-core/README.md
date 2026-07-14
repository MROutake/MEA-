# MEA Network Core

Transport-neutraler Netzwerk-Stack für die MEA-Plattform. Verbindet das
Protokoll (`mea-protocol`) mit einem beliebigen verbindungsorientierten
Byte-Transport, ohne ein konkretes Medium (WS500, WLAN, TCP) zu kennen.

## Abhängigkeiten

- `mea-core`, `mea-protocol`. Kein konkreter Transport, kein dynamischer Speicher.

## Bausteine

| Typ | Rolle |
|-----|-------|
| `INetworkTransport` | Verbindungsorientierter Byte-Transport (`connect`/`disconnect`/`linkState` + nicht blockierendes `read`/`write`). |
| `NetworkSession` | Zustandsmaschine `Disconnected → Connecting → Online → Backoff → Fault` mit `ReconnectPolicy` (exponentieller Backoff, Connect-Timeout, `maxAttempts`). |
| `ProtocolBridge<Tx,Rx,RxFrames>` | Kodiert/dekodiert Frames, hält TX-Byte-FIFO und RX-Byte-Puffer + Frame-Queue, pumpt bei Online-Verbindung nicht blockierend. |
| `NetworkMeasurementSink` | `IMeasurementSink`, der Messwerte als Nachrichten publiziert – fügt sich unverändert in die Pipeline ein. |
| `NetworkMetrics` | `reconnectCount`, `txDropCount`, `rxErrorCount`, `txFrameCount`, `rxFrameCount`, `connectAttempts`. |
| `testing::FakeNetworkTransport` | Fake mit Connect-Simulation, Loopback und injizierbaren Fehlern. |

## Puffer-Strategie

- **TX:** feste Byte-FIFO. `publish()` kodiert den Frame direkt hinein; passt er
  nicht, wird **nur der neue Frame** verworfen (`txDropCount++`, Rückgabe
  `WouldBlock`) – bestehende Frames bleiben intakt.
- **RX:** Bytes werden in einen linearen Puffer gelesen; vollständige Frames
  werden in eine feste Frame-Queue decodiert. Defekte Frames (Magic/Version/CRC/
  Länge) werden byteweise übersprungen (`rxErrorCount++`).

## Reihenfolge im Composition Root

```cpp
session.begin(now);
session.connect(now);   // Verbindung anfordern
bridge.begin();
// je Zyklus:
bridge.update(now);     // treibt session + I/O (oder via sink.update() im ApplyOutputs-Schritt)
```

`NetworkMeasurementSink::update()` ruft `bridge.update()` auf; der Sink kann
daher wie jeder andere Sink über `SinkManager::updateAll()` im
`ApplyOutputs`-Schritt des Orchestrators getrieben werden.

## Tests

```bash
pio test -e native
```

Deckt Backoff-Berechnung, alle Session-Übergänge (Connect, Timeout, Backoff,
Reconnect nach Verbindungsverlust, Fault nach `maxAttempts`), Bridge-Roundtrip
(Loopback), TX-Overflow und RX-Fehler sowie die End-to-End-Kette
Messwert → Sink → Bridge → Transport ab.
