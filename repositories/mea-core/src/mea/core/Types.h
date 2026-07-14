#pragma once

/// @file Types.h
/// @brief Grundtypen der MEA-Plattform: IDs, Zeitstempel, rollover-sichere
/// Zeitvergleiche.

#include <stdint.h>

namespace mea {

/// Eindeutige Kennung einer Komponente. ID 0 ist ungültig und reserviert.
using ComponentId = uint16_t;

/// Millisekunden-Zeitstempel (z. B. millis()). Läuft nach ~49,7 Tagen über;
/// alle Vergleiche müssen über elapsedMs()/intervalElapsed() erfolgen.
using TimestampMs = uint32_t;

/// Monoton steigende Sequenznummer je Messquelle. Überlauf ist erlaubt.
using SequenceNumber = uint32_t;

/// Reservierte, ungültige Komponenten-ID.
constexpr ComponentId InvalidComponentId = 0;

/// Rollover-sichere vergangene Zeit seit @p sinceMs (unsigned Differenz).
[[nodiscard]] constexpr TimestampMs elapsedMs(const TimestampMs nowMs,
                                              const TimestampMs sinceMs) noexcept {
    return static_cast<TimestampMs>(nowMs - sinceMs);
}

/// True, wenn seit @p sinceMs mindestens @p intervalMs vergangen sind (rollover-sicher).
[[nodiscard]] constexpr bool intervalElapsed(const TimestampMs nowMs,
                                             const TimestampMs sinceMs,
                                             const TimestampMs intervalMs) noexcept {
    return elapsedMs(nowMs, sinceMs) >= intervalMs;
}

}  // namespace mea
