#pragma once

/// @file Ws500Config.h
/// @brief Konfiguration des WS500-Transports. Reine Datenstruktur; keine
///        Hardware-Initialisierung (die passiert in Ws500Transport::begin()).

#include <cstdint>

#include <MeaCore.h>

namespace mea {
namespace network {
namespace ws500 {

struct Ws500Config {
    /// Komponenten-ID des Transports (≠ 0).
    ComponentId id{InvalidComponentId};
    /// Ziel-IPv4-Adresse (host[0].host[1].host[2].host[3]).
    std::uint8_t host[4]{0, 0, 0, 0};
    /// Ziel-Port.
    std::uint16_t port{0};
    /// Lokaler Port (0 = automatisch).
    std::uint16_t localPort{0};
    /// Maximale Dauer eines Verbindungsaufbaus, danach Error (nicht blockierend).
    TimestampMs connectTimeoutMs{5000};
};

}  // namespace ws500
}  // namespace network
}  // namespace mea
