#pragma once

/// @file IEspNowRadio.h
/// @brief Hardwareabstraktion für das ESP-NOW-Funkmodul. Trennt Client/Server
///        von esp_now/WiFi und macht die Verbindungslogik nativ testbar.
///
/// Der Empfang läuft hardwareseitig in einem WiFi-Task; Implementierungen
/// puffern eingehende Datagramme intern und liefern sie über available()/
/// receive() im Aufruferkontext aus (nicht blockierend).

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "EspNowTypes.h"

namespace mea {

class IEspNowRadio {
public:
    virtual ~IEspNowRadio() = default;

    /// Initialisiert WiFi (STA) und ESP-NOW. Keine Hardwarearbeit im Konstruktor.
    virtual Status begin() noexcept = 0;

    /// Wechselt den WiFi-Kanal (1..maximumChannel()); nur ohne AP-Verbindung.
    virtual Status setChannel(std::uint8_t channel) noexcept = 0;
    [[nodiscard]] virtual std::uint8_t channel() const noexcept = 0;
    /// Höchster nutzbarer Kanal (Region: EU 13, USA 11).
    [[nodiscard]] virtual std::uint8_t maximumChannel() const noexcept = 0;

    [[nodiscard]] virtual MacAddress localAddress() const noexcept = 0;

    /// Sendet ein Datagramm (Peer-Verwaltung ist Sache der Implementierung).
    virtual Status send(const MacAddress& destination, const std::uint8_t* data,
                        std::size_t size) noexcept = 0;

    /// Entfernt einen Peer (z. B. abgelaufener Client); unbekannte Peers sind ok.
    virtual Status removePeer(const MacAddress& address) noexcept = 0;

    [[nodiscard]] virtual std::size_t available() const noexcept = 0;
    virtual Status receive(EspNowPacket& output) noexcept = 0;
};

}  // namespace mea
