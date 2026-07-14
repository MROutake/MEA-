#pragma once

/// @file PipelineTypes.h
/// @brief Konfigurations- und Zustandstypen der Messwert-Pipeline (ADR 0005).

#include <cstdint>

#include <MeaCore.h>

namespace mea {

/// Wiederholungsverhalten bei Zyklusfehlern.
struct RetryPolicy {
    /// Wartezeit vor einem erneuten Versuch.
    TimestampMs delayMs{0};
    /// Maximale Anzahl Wiederholungen je Zyklus (0 = keine Wiederholungen).
    std::uint16_t maximumAttempts{0};
};

/// Statische Beschreibung einer Pipeline. Die ID-Arrays (processorIds/sinkIds)
/// liegen im Composition Root und müssen länger leben als die State Machine
/// (ADR 0001).
struct PipelineConfig {
    ComponentId pipelineId{InvalidComponentId};
    /// Genau eine Messquelle.
    ComponentId sourceId{InvalidComponentId};
    /// 0..kMaxProcessors Prozessoren, in Ausführungsreihenfolge.
    ArrayView<const ComponentId> processorIds{};
    /// 1..kMaxSinks Sinks; ein Zyklus ist erst erfolgreich, wenn alle
    /// konfigurierten Sinks den Wert übernommen haben (ADR 0005).
    ArrayView<const ComponentId> sinkIds{};
    /// Abstand zwischen den Startzeitpunkten zweier Zyklen (> 0).
    TimestampMs cycleIntervalMs{1000};
    /// Maximale Wartezeit auf einen Messwert (> 0).
    TimestampMs acquisitionTimeoutMs{1000};
    /// Maximale Zeit für die Übergabe an alle Sinks (> 0).
    TimestampMs publishTimeoutMs{1000};
    RetryPolicy retry{};
    /// false: Pipeline startet in Disabled und wartet auf enable().
    bool startImmediately{true};
};

/// Zustände der Pipeline-Maschine.
enum class PipelineState : std::uint8_t {
    Uninitialized = 0,
    Disabled,
    WaitingForCycle,
    WaitingForMeasurement,
    Processing,
    Publishing,
    Backpressure,
    RetryDelay,
    Fault
};

/// Menschlich lesbarer Zustandsname (statisches Stringliteral).
[[nodiscard]] constexpr const char* pipelineStateName(
    const PipelineState state) noexcept {
    switch (state) {
        case PipelineState::Uninitialized:
            return "Uninitialized";
        case PipelineState::Disabled:
            return "Disabled";
        case PipelineState::WaitingForCycle:
            return "WaitingForCycle";
        case PipelineState::WaitingForMeasurement:
            return "WaitingForMeasurement";
        case PipelineState::Processing:
            return "Processing";
        case PipelineState::Publishing:
            return "Publishing";
        case PipelineState::Backpressure:
            return "Backpressure";
        case PipelineState::RetryDelay:
            return "RetryDelay";
        case PipelineState::Fault:
            return "Fault";
    }
    return "Unknown";
}

}  // namespace mea
