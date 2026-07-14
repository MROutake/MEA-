#pragma once

/// @file NetworkSession.h
/// @brief Nicht blockierende Verbindungs-Zustandsmaschine über einem
///        INetworkTransport. Zustände: Disconnected, Connecting, Online,
///        Backoff, Fault. Reconnect mit exponentiellem Backoff; ein einzelner
///        update() leistet höchstens einen Übergang plus begrenzte Arbeit.

#include <cstdint>

#include <MeaCore.h>

#include "INetworkTransport.h"
#include "NetworkMetrics.h"
#include "ReconnectPolicy.h"

namespace mea {
namespace network {

class NetworkSession final {
public:
    enum class State : std::uint8_t {
        Disconnected = 0,  ///< Kein Verbindungswunsch aktiv.
        Connecting,        ///< Verbindungsaufbau läuft (mit Timeout).
        Online,            ///< Verbunden; Daten fließen.
        Backoff,           ///< Wartet vor dem nächsten Versuch.
        Fault              ///< maxAttempts erschöpft; nur begin()/connect() löst.
    };

    /// @param transport Nicht besitzend; muss die Session überleben (ADR 0001).
    /// @param metrics   Gemeinsame Zähler (auch von der Bridge genutzt).
    NetworkSession(INetworkTransport& transport, const ReconnectPolicy& policy,
                   NetworkMetrics& metrics, ComponentId id) noexcept;

    /// Initialisiert den Transport und setzt die Session auf Disconnected.
    Status begin(TimestampMs nowMs) noexcept;

    /// Fordert eine Online-Verbindung an (setzt Fehlversuche zurück).
    Status connect(TimestampMs nowMs) noexcept;

    /// Beendet den Verbindungswunsch und trennt den Transport.
    Status disconnect() noexcept;

    /// Treibt die Zustandsmaschine an (nicht blockierend).
    Status update(TimestampMs nowMs) noexcept;

    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] bool isOnline() const noexcept { return state_ == State::Online; }
    [[nodiscard]] ComponentId id() const noexcept { return id_; }
    [[nodiscard]] Status lastStatus() const noexcept { return lastStatus_; }

    [[nodiscard]] static const char* stateName(State state) noexcept;

private:
    void enterConnecting(TimestampMs nowMs) noexcept;
    void enterOnline(TimestampMs nowMs) noexcept;
    void enterBackoff(TimestampMs nowMs) noexcept;
    void handleFailure(Status cause, TimestampMs nowMs) noexcept;

    INetworkTransport& transport_;
    ReconnectPolicy policy_;
    NetworkMetrics& metrics_;
    ComponentId id_;

    State state_{State::Disconnected};
    Status lastStatus_{StatusCode::NotInitialized, InvalidComponentId, 0};
    bool initialized_{false};
    bool wantOnline_{false};

    std::uint16_t attempts_{0};
    TimestampMs backoffMs_{0};
    TimestampMs connectStartMs_{0};
    TimestampMs backoffStartMs_{0};
};

}  // namespace network
}  // namespace mea
