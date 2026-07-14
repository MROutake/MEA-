#pragma once

/// @file ReconnectPolicy.h
/// @brief Wiederverbindungsstrategie mit exponentiellem Backoff und Timeout
///        für die Connecting-Phase (siehe NetworkSession).

#include <cstdint>

#include <MeaCore.h>

namespace mea {
namespace network {

struct ReconnectPolicy {
    /// Wartezeit vor dem ersten erneuten Versuch nach einem Fehlschlag.
    TimestampMs initialBackoffMs{500};
    /// Obergrenze der Backoff-Wartezeit.
    TimestampMs maxBackoffMs{8000};
    /// Faktor für die exponentielle Steigerung (>= 1).
    std::uint16_t backoffMultiplier{2};
    /// Maximale Anzahl aufeinanderfolgender Fehlversuche (0 = unbegrenzt).
    std::uint16_t maxAttempts{0};
    /// Maximale Dauer eines einzelnen Verbindungsaufbaus, danach Abbruch.
    TimestampMs connectTimeoutMs{3000};
};

/// Nächste Backoff-Wartezeit (gekappt auf maxBackoffMs). @p current ist die
/// zuletzt verwendete Wartezeit; 0 liefert die erste (initialBackoffMs).
[[nodiscard]] constexpr TimestampMs nextBackoff(const ReconnectPolicy& policy,
                                                const TimestampMs current) noexcept {
    if (current == 0) {
        return policy.initialBackoffMs;
    }
    const TimestampMs multiplier =
        policy.backoffMultiplier == 0 ? 1U : policy.backoffMultiplier;
    const TimestampMs scaled = current * multiplier;
    if (scaled > policy.maxBackoffMs || scaled < current) {  // Kappung oder Overflow
        return policy.maxBackoffMs;
    }
    return scaled;
}

}  // namespace network
}  // namespace mea
