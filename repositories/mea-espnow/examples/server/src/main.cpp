/// ESP-NOW-Server-Beispiel: nimmt Verbindungen von Sensor-Clients an
/// (automatische Discovery, keine MAC-Konfiguration) und gibt empfangene
/// Messwert-Frames (CSV-Zeilen) auf Serial aus.

#include <Arduino.h>
#include <MeaEspNow.h>

namespace {

constexpr std::uint8_t kChannel = 1;               // Arbeitskanal des Servers
constexpr mea::TimestampMs kClientTimeoutMs = 5000;

mea::ArduinoEspNowRadio radio;  // Kanalbereich 1..13 (EU)
mea::EspNowServer server(radio, {kChannel, kClientTimeoutMs});

void printMac(const mea::MacAddress& address) {
    for (std::size_t index = 0; index < 6; ++index) {
        if (index > 0) {
            Serial.print(':');
        }
        Serial.printf("%02X", address.bytes[index]);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2000);  // Zeit, damit der Serial Monitor sich verbinden kann

    const mea::Status status = server.begin();
    Serial.print("[server] begin: ");
    Serial.print(mea::statusCodeName(status.code));
    Serial.print(" | MAC ");
    printMac(radio.localAddress());
    Serial.printf(" | Kanal %u\n", radio.channel());
}

void loop() {
    (void)server.update(millis());

    mea::EspNowDataFrame frame{};
    while (server.available() > 0 && server.read(frame).ok()) {
        printMac(frame.source);
        Serial.print(" > ");
        Serial.write(frame.payload, frame.length);  // CSV-Zeile inkl. '\n'
    }

    static std::uint32_t lastStatusMs = 0;
    if (millis() - lastStatusMs >= 5000) {
        lastStatusMs = millis();
        Serial.printf("[server] verbundene Clients: %u\n",
                      static_cast<unsigned>(server.clientCount()));
    }
}
