#pragma once

/// @file EspNowClient.h
/// @brief ESP-NOW-Client (Sensorseite): findet einen Server automatisch ohne
///        hardcodierte MAC-Adresse oder Kanal, führt die Verbindung und
///        sendet Data-Frames (ADR 0008).
///
/// Nicht blockierende Statemachine (ADR 0004), angetrieben über update():
///
///     Scanning ──Offer──> Connecting ──ConnectAccept──> Connected
///        ^                    │ Timeout                     │ Pong-Ausfall
///        └────────────────────┴──────────────────────────────┘
///
/// - Scanning: je Kanal (1..maximumChannel) ein Discover-Broadcast mit
///   Verweildauer channelDwellMs; nach einem Verbindungsabriss beginnt der
///   Scan auf dem zuletzt erfolgreichen Kanal.
/// - Connected: Ping alle pingIntervalMs; bleiben mehr als
///   maximumMissedPongs Antworten aus, wird neu verbunden (reconnects()).
///
/// Implementiert IDevice (Registrierung im MeasurementNode) und
/// IEspNowSession (Sender-Sicht für den EspNowMeasurementSink).

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "EspNowTypes.h"
#include "IEspNowRadio.h"

namespace mea {

class EspNowClient final : public IDevice, public IEspNowSession {
public:
    enum class State : std::uint8_t { Uninitialized = 0, Scanning, Connecting, Connected };

    struct Config {
        /// Verweildauer je Kanal während der Suche (> 0).
        TimestampMs channelDwellMs{150};
        /// Maximale Wartezeit auf ConnectAccept (> 0).
        TimestampMs connectTimeoutMs{500};
        /// Heartbeat-Intervall im verbundenen Zustand (> 0).
        TimestampMs pingIntervalMs{1000};
        /// Verbindungsabbruch nach so vielen ausbleibenden Pongs (> 0).
        std::uint8_t maximumMissedPongs{3};
        /// Startkanal der Suche (1..maximumChannel des Radios).
        std::uint8_t firstChannel{1};
    };

    /// @param radio Nicht besitzende Referenz; muss den Client überleben (ADR 0001).
    EspNowClient(IEspNowRadio& radio, const Config& config) noexcept;

    /// Reinitialisierend: startet die Suche neu; Diagnosezähler laufen weiter.
    Status begin() noexcept override;
    Status update(TimestampMs nowMs) noexcept override;

    [[nodiscard]] bool connected() const noexcept override {
        return state_ == State::Connected;
    }
    Status sendData(const std::uint8_t* payload, std::size_t size) noexcept override;

    [[nodiscard]] State state() const noexcept { return state_; }
    /// MAC des verbundenen Servers (nur gültig, solange connected()).
    [[nodiscard]] const MacAddress& serverAddress() const noexcept {
        return serverAddress_;
    }
    /// Diagnosezähler.
    [[nodiscard]] std::uint32_t reconnects() const noexcept { return reconnects_; }
    [[nodiscard]] std::uint32_t sentDataFrames() const noexcept {
        return sentDataFrames_;
    }
    [[nodiscard]] std::uint32_t sendFailures() const noexcept { return sendFailures_; }
    [[nodiscard]] std::uint32_t protocolErrors() const noexcept {
        return protocolErrors_;
    }

private:
    void drainReceive(TimestampMs nowMs) noexcept;
    Status updateScanning(TimestampMs nowMs) noexcept;
    void enterScanning() noexcept;
    Status sendControl(EspNowMessageType type, const MacAddress& destination) noexcept;

    IEspNowRadio& radio_;
    Config config_;

    State state_{State::Uninitialized};
    MacAddress serverAddress_{};
    std::uint8_t scanChannel_{1};
    bool switchChannelNow_{true};

    TimestampMs dwellStartMs_{0};
    TimestampMs connectStartMs_{0};
    TimestampMs lastPingSentMs_{0};
    TimestampMs lastPongMs_{0};

    std::uint32_t reconnects_{0};
    std::uint32_t sentDataFrames_{0};
    std::uint32_t sendFailures_{0};
    std::uint32_t protocolErrors_{0};
    bool initialized_{false};
};

}  // namespace mea
