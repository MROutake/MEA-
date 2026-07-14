#pragma once

/// @file INetworkTransport.h
/// @brief Verbindungsorientierter, nicht blockierender Byte-Transport für
///        Netzwerke. Erweitert die Byte-Transport-Idee (ADR 0006) um einen
///        expliziten Verbindungslebenszyklus (connect/disconnect/linkState).
///        Der Transport bewegt nur Bytes und kennt das Protokoll nicht.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

namespace mea {
namespace network {

/// Zustand der physischen/logischen Verbindung eines Transports.
enum class LinkState : std::uint8_t {
    Down = 0,    ///< Getrennt, kein Verbindungsaufbau aktiv.
    Connecting,  ///< Verbindungsaufbau läuft (nicht blockierend).
    Up,          ///< Verbunden, Daten können fließen.
    Error        ///< Aufbau/Verbindung fehlgeschlagen.
};

[[nodiscard]] constexpr const char* linkStateName(const LinkState state) noexcept {
    switch (state) {
        case LinkState::Down:
            return "Down";
        case LinkState::Connecting:
            return "Connecting";
        case LinkState::Up:
            return "Up";
        case LinkState::Error:
            return "Error";
    }
    return "Unknown";
}

/// Nicht blockierender, verbindungsorientierter Byte-Transport.
class INetworkTransport {
public:
    virtual ~INetworkTransport() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;

    /// Initialisiert die Hardware/den Stack (reinitialisierend, ADR 0004).
    virtual Status begin() noexcept = 0;

    /// Startet den Verbindungsaufbau nicht blockierend. Der Fortschritt wird
    /// über update()/linkState() sichtbar.
    virtual Status connect(TimestampMs nowMs) noexcept = 0;

    /// Trennt die Verbindung; danach ist linkState() == Down.
    virtual Status disconnect() noexcept = 0;

    [[nodiscard]] virtual LinkState linkState() const noexcept = 0;

    /// Treibt den Transport an (begrenzte Arbeit, blockiert nie).
    virtual Status update(TimestampMs nowMs) noexcept = 0;

    /// Bytes, die write() aktuell ohne Blockieren annimmt (darf 0 sein).
    [[nodiscard]] virtual std::size_t writable() const noexcept = 0;

    /// Schreibt höchstens @p size Bytes; @p written meldet die übernommene Anzahl.
    virtual Status write(const std::uint8_t* data, std::size_t size,
                         std::size_t& written) noexcept = 0;

    [[nodiscard]] virtual std::size_t readable() const noexcept = 0;

    /// Liest höchstens @p capacity Bytes; @p readCount meldet die gelesene Anzahl.
    virtual Status read(std::uint8_t* data, std::size_t capacity,
                        std::size_t& readCount) noexcept = 0;
};

}  // namespace network
}  // namespace mea
