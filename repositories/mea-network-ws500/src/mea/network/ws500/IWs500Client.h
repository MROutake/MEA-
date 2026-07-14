#pragma once

/// @file IWs500Client.h
/// @brief Hardware-Abstraktion des WS500-Moduls (Socket-Ebene). Ermöglicht
///        native Tests (FakeWs500Client) und hält Ws500Transport frei von
///        konkreten SPI-/Treiberdetails. Enthält keine Protokoll-Logik.

#include <cstddef>
#include <cstdint>

#include "Ws500Config.h"
#include "Ws500Result.h"

namespace mea {
namespace network {
namespace ws500 {

/// Minimaler, nicht blockierender Socket-Client für das WS500-Modul.
class IWs500Client {
public:
    virtual ~IWs500Client() = default;

    /// Initialisiert Modul/Socket mit @p config (reinitialisierend).
    virtual Ws500Result begin(const Ws500Config& config) noexcept = 0;

    /// Startet den Verbindungsaufbau (nicht blockierend). Fortschritt über
    /// connected()/faulted() abfragen.
    virtual Ws500Result startConnect(TimestampMs nowMs) noexcept = 0;

    /// True, sobald die Verbindung steht.
    [[nodiscard]] virtual bool connected() const noexcept = 0;

    /// True bei gelatchtem Hardware-/Modulfehler.
    [[nodiscard]] virtual bool faulted() const noexcept = 0;

    /// Schließt die Verbindung/den Socket.
    virtual void close() noexcept = 0;

    /// Bytes, die send() aktuell annimmt (darf 0 sein).
    [[nodiscard]] virtual std::size_t writable() const noexcept = 0;

    /// Sendet höchstens @p size Bytes; @p sent meldet die übernommene Anzahl.
    virtual Ws500Result send(const std::uint8_t* data, std::size_t size,
                             std::size_t& sent) noexcept = 0;

    /// Anzahl empfangsbereiter Bytes.
    [[nodiscard]] virtual std::size_t readable() const noexcept = 0;

    /// Liest höchstens @p capacity Bytes; @p received meldet die gelesene Anzahl.
    virtual Ws500Result recv(std::uint8_t* data, std::size_t capacity,
                             std::size_t& received) noexcept = 0;
};

}  // namespace ws500
}  // namespace network
}  // namespace mea
