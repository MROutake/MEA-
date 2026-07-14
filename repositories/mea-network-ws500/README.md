# MEA Network WS500

WS500-Transportadapter für den MEA-Netzwerk-Stack. Implementiert
`INetworkTransport` (aus `mea-network-core`) und bewegt ausschließlich Bytes –
**keine** Protokoll-Encode/Decode-Logik (harte Architekturregel).

## Abhängigkeiten

- `mea-core`, `mea-network-core`. Kein `mea-protocol` (bewusst: der Transport
  kennt das Protokoll nicht).

## Bausteine

| Typ | Rolle |
|-----|-------|
| `Ws500Config` | Ziel-IP/Port, lokaler Port, Connect-Timeout, Komponenten-ID. |
| `IWs500Client` | Hardware-Abstraktion (Socket-Ebene): `begin`, `startConnect`, `connected`, `faulted`, `send`, `recv`, `close`. |
| `Ws500Transport` | `INetworkTransport` über einem `IWs500Client`; nicht blockierender Aufbau mit Timeout, Link-Loss-Erkennung, Fehler-Mapping. |
| `Ws500Result` + `statusFromWs500()` | Abbildung hardwarenaher Ergebnisse auf `mea::Status` (ADR 0002). |
| `ArduinoWs500Client<ClientT>` | Referenz-Client über einen Arduino-`Client` (z. B. `EthernetClient`); nur unter `ARDUINO`. |
| `testing::FakeWs500Client` | Fake für native Tests (Connect-Simulation, Loopback, injizierbare Fehler). |

## Fehler-Mapping

| `Ws500Result` | `mea::StatusCode` |
|---------------|-------------------|
| `Ok` | `Ok` |
| `WouldBlock` | `WouldBlock` (transient) |
| `NotReady` | `Busy` (transient) |
| `Timeout` | `Timeout` |
| `Disconnected` | `IoError` |
| `HardwareFault` | `IoError` |

Harte Fehler (nicht transient) setzen den Transport intern auf `LinkState::Error`;
die `NetworkSession` baut daraufhin per Backoff neu auf.

## Reale Hardware anbinden

`ArduinoWs500Client` templatisiert über einen Arduino-`Client`-kompatiblen Typ
(kein Zwang zu einer bestimmten Netzwerk-Library):

```cpp
#include <Ethernet.h>            // WS500/W5500-Treiber der Wahl
EthernetClient ethClient;
mea::network::ws500::ArduinoWs500Client<EthernetClient> client(ethClient);
mea::network::ws500::Ws500Transport transport(client, ws500Config);
```

Beispielverdrahtung mit vollständiger Pipeline:
[examples/demo-pipeline-integration/main.cpp](examples/demo-pipeline-integration/main.cpp).

## Offene Punkte

- Die Arduino-`Client::connect()`-API kann je nach Stack kurz blockieren; für
  streng nicht blockierenden Aufbau einen modul-spezifischen Client
  implementieren.

## Tests

```bash
pio test -e native
```

Deckt Transport-Lebenszyklus (Connect, Timeout, Verbindungsverlust,
Hardware-Fault, ungültige Konfiguration), das Fehler-Mapping und den vollen
Stack (Session + Bridge + Codec) inklusive Reconnect ab.
