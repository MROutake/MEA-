#pragma once

/// @file Ws500Result.h
/// @brief Hardwarenahe Ergebniscodes der WS500-Clientschicht und ihre
///        Abbildung auf das bestehende MEA-Statusmodell (ADR 0002).

#include <cstdint>

#include <MeaCore.h>

namespace mea {
namespace network {
namespace ws500 {

/// Ergebnis einer WS500-Client-Operation (Socket-/Modulebene).
enum class Ws500Result : std::uint8_t {
    Ok = 0,        ///< Erfolg.
    WouldBlock,    ///< Aktuell kein Platz/keine Daten (nicht blockieren).
    NotReady,      ///< Modul/Socket noch nicht bereit.
    Timeout,       ///< Zeitüberschreitung.
    Disconnected,  ///< Verbindung verloren/geschlossen.
    HardwareFault  ///< SPI-/Modulfehler.
};

/// Bildet ein Ws500Result auf einen mea::Status ab (@p origin = Transport-ID).
[[nodiscard]] constexpr Status statusFromWs500(const Ws500Result result,
                                               const ComponentId origin,
                                               const std::uint16_t detail = 0) noexcept {
    switch (result) {
        case Ws500Result::Ok:
            return okStatus();
        case Ws500Result::WouldBlock:
            return makeStatus(StatusCode::WouldBlock, origin, detail);
        case Ws500Result::NotReady:
            return makeStatus(StatusCode::Busy, origin, detail);
        case Ws500Result::Timeout:
            return makeStatus(StatusCode::Timeout, origin, detail);
        case Ws500Result::Disconnected:
            return makeStatus(StatusCode::IoError, origin, detail);
        case Ws500Result::HardwareFault:
            return makeStatus(StatusCode::IoError, origin, detail);
    }
    return makeStatus(StatusCode::InternalError, origin, detail);
}

[[nodiscard]] constexpr const char* ws500ResultName(const Ws500Result result) noexcept {
    switch (result) {
        case Ws500Result::Ok:
            return "Ok";
        case Ws500Result::WouldBlock:
            return "WouldBlock";
        case Ws500Result::NotReady:
            return "NotReady";
        case Ws500Result::Timeout:
            return "Timeout";
        case Ws500Result::Disconnected:
            return "Disconnected";
        case Ws500Result::HardwareFault:
            return "HardwareFault";
    }
    return "Unknown";
}

}  // namespace ws500
}  // namespace network
}  // namespace mea
