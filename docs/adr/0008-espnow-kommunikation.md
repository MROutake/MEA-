# ADR 0008 – ESP-NOW-Kommunikation (mea-espnow)

Status: akzeptiert · Datum: 2026-07-19

## Kontext

Der Sensor-Node soll Messwerte drahtlos an einen Empfänger senden. ESP-NOW
ist datagrammbasiert (max. 250 Bytes je Frame, verbindungslos, MAC-Layer-ACK)
und passt damit nicht auf die Stream-Abstraktion `IByteTransport` (ADR 0006):
Bei Paketverlust würde ein zeilenbasierter Strom unbrauchbar zerreißen.
Außerdem sollen MAC-Adressen und WiFi-Kanäle nicht konfiguriert werden
müssen, und Sensoren und Server sollen eine geführte Verbindung haben.

## Entscheidung

1. **Eigenes Repo `mea-espnow`** mit einem Message-Pfad statt Stream:
   1 Messwert = 1 Datagramm (kodiert über den vorhandenen
   `IMeasurementEncoder` aus mea-communication, Standard: CSV). Verluste
   kosten einzelne Messwerte und sind über Sequenzlücken erkennbar (ADR 0003).
2. **Protokoll** mit 4-Byte-Header (Magic 'MN', Version, Typ) und
   Client-Server-Rollen:
   - Discovery: Der Client scannt die Kanäle (1..13) und broadcastet
     `Discover`; der Server antwortet mit `Offer`. MAC-Adressen lernen beide
     Seiten aus dem Empfang — nichts ist hardcodiert.
   - Verbindung: `ConnectRequest`/`ConnectAccept`; danach Heartbeat
     `Ping`/`Pong`. Bleiben Pongs aus, verbindet der Client automatisch neu;
     der Server entfernt Clients ohne Lebenszeichen (clientTimeoutMs) und
     nimmt unbekannte Absender von Ping/Data implizit wieder auf (robust
     gegen Neustarts beider Seiten).
3. **HAL `IEspNowRadio`** trennt die Verbindungslogik von esp_now/WiFi:
   Client, Server und Sink sind nativ testbar (FakeEspNowRadio);
   `ArduinoEspNowRadio` (nur ESP32) übergibt Empfangspakete aus dem
   WiFi-Task über eine Spinlock-geschützte Queue.
4. **`EspNowClient` und `EspNowServer` implementieren `IDevice`** (ADR 0007)
   und fügen sich damit ohne Sonderbehandlung in den `MeasurementNode` ein.
5. **`EspNowMeasurementSink` mit Drop-Oldest-Policy** (bewusste Abweichung
   vom WouldBlock-Backpressure des `BufferedMeasurementSink`): Die
   Funkstrecke ist inhärent verlustbehaftet; ist kein Server verbunden,
   verdrängt der neueste Messwert den ältesten (gezählt über
   `droppedMeasurements()`). Die Pipelines geraten dadurch nicht in
   Backpressure, Serial läuft unbeeinflusst weiter.
6. **Unverschlüsselt (v1)**: PMK/LMK-Verschlüsselung ist bewusst außerhalb
   des Umfangs und als Erweiterung vorgesehen.

## Konsequenzen

- Der Sensor-Node publiziert jede Pipeline an zwei Sinks (Serial-Text und
  ESP-NOW-CSV); ohne erreichbaren Server ändert sich am Knoten nichts
  Sichtbares außer Diagnosezählern.
- Ein Empfänger braucht nur `EspNowServer` + Radio (siehe
  repositories/mea-espnow/examples/server) und bekommt CSV-Zeilen samt
  Absender-MAC.
- Das Protokoll ist über das Versionsfeld im Header erweiterbar (z. B.
  Verschlüsselung, bidirektionale Kommandos über `LineCommandDecoder`).
