#pragma once

/// @file EspNowServer.h
/// @brief ESP-NOW-Server (Empfängerseite): wartet auf Clients, beantwortet
///        Discovery, führt die Verbindungen und sammelt Data-Frames
///        (ADR 0008). Implementiert IDevice.
///
/// Verhalten (nicht blockierend, ADR 0004):
/// - Discover -> Offer (an die gelernte Absender-MAC, keine Konfiguration).
/// - ConnectRequest -> Client in die Tabelle aufnehmen + ConnectAccept.
/// - Ping -> lastSeen aktualisieren + Pong. Unbekannte Absender von Ping/Data
///   werden implizit wieder aufgenommen (robust gegen Server-Neustart).
/// - Data -> Nutzlast in die Empfangsqueue legen (bei voller Queue wird der
///   NEUE Frame verworfen und droppedFrames() erhöht).
/// - Clients ohne Lebenszeichen länger als clientTimeoutMs werden entfernt
///   (inkl. Funk-Peer).

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "EspNowTypes.h"
#include "IEspNowRadio.h"

namespace mea {

class EspNowServer final : public IDevice {
public:
    /// Maximale Anzahl gleichzeitig verbundener Clients.
    static constexpr std::size_t kMaxClients = 8;
    /// Kapazität der Empfangsqueue für Data-Frames.
    static constexpr std::size_t kDataQueueCapacity = 4;

    struct ClientInfo {
        MacAddress address{};
        TimestampMs lastSeenMs{0};
        bool used{false};
    };

    struct Config {
        /// WiFi-Kanal, auf dem der Server arbeitet (1..maximumChannel).
        /// Clients finden den Kanal selbst über ihren Scan.
        std::uint8_t channel{1};
        /// Client-Ablauf ohne Lebenszeichen (> 0).
        TimestampMs clientTimeoutMs{5000};
    };

    /// @param radio Nicht besitzende Referenz; muss den Server überleben (ADR 0001).
    EspNowServer(IEspNowRadio& radio, const Config& config) noexcept;

    /// Reinitialisierend: leert Client-Tabelle und Empfangsqueue;
    /// Diagnosezähler laufen weiter.
    Status begin() noexcept override;
    Status update(TimestampMs nowMs) noexcept override;

    /// Anzahl aktuell verbundener Clients.
    [[nodiscard]] std::size_t clientCount() const noexcept;
    /// Client-Eintrag in Tabellenreihenfolge (used == false bei freiem Slot).
    [[nodiscard]] const ClientInfo& client(std::size_t index) const noexcept {
        return clients_[index < kMaxClients ? index : 0];
    }

    /// Empfangene Data-Frames (FIFO).
    [[nodiscard]] std::size_t available() const noexcept { return dataQueue_.size(); }
    Status read(EspNowDataFrame& output) noexcept;

    /// Diagnosezähler.
    [[nodiscard]] std::uint32_t droppedFrames() const noexcept { return droppedFrames_; }
    [[nodiscard]] std::uint32_t rejectedClients() const noexcept {
        return rejectedClients_;
    }
    [[nodiscard]] std::uint32_t expiredClients() const noexcept {
        return expiredClients_;
    }
    [[nodiscard]] std::uint32_t protocolErrors() const noexcept {
        return protocolErrors_;
    }

private:
    /// Nimmt @p address in die Tabelle auf bzw. aktualisiert lastSeen.
    /// false, wenn die Tabelle voll ist und der Client neu wäre.
    bool touchClient(const MacAddress& address, TimestampMs nowMs) noexcept;
    void expireClients(TimestampMs nowMs) noexcept;
    void handlePacket(const EspNowPacket& packet, EspNowMessageType type,
                      TimestampMs nowMs) noexcept;
    Status sendControl(EspNowMessageType type, const MacAddress& destination) noexcept;

    IEspNowRadio& radio_;
    Config config_;

    ClientInfo clients_[kMaxClients]{};
    RingBuffer<EspNowDataFrame, kDataQueueCapacity> dataQueue_{};

    std::uint32_t droppedFrames_{0};
    std::uint32_t rejectedClients_{0};
    std::uint32_t expiredClients_{0};
    std::uint32_t protocolErrors_{0};
    bool initialized_{false};
};

}  // namespace mea
