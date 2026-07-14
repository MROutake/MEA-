#pragma once

/// @file NetworkMetrics.h
/// @brief Diagnosezähler für Session und ProtocolBridge (trivial kopierbar).

#include <cstdint>

namespace mea {
namespace network {

struct NetworkMetrics {
    /// Verlorene und wiederaufgebaute Online-Verbindungen.
    std::uint32_t reconnectCount{0};
    /// Wegen vollem TX-Puffer verworfene Frames (kein stiller Verlust:
    /// publish() meldet zusätzlich WouldBlock).
    std::uint32_t txDropCount{0};
    /// Fehlerhafte/verworfene Frames beim Empfang (Magic/Version/CRC/Länge).
    std::uint32_t rxErrorCount{0};
    /// Erfolgreich in den Transport geschriebene Frames.
    std::uint32_t txFrameCount{0};
    /// Erfolgreich decodierte Frames.
    std::uint32_t rxFrameCount{0};
    /// Anzahl gestarteter Verbindungsversuche.
    std::uint32_t connectAttempts{0};
};

}  // namespace network
}  // namespace mea
