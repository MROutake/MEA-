#pragma once

/// @file MeasurementPipelineMachine.h
/// @brief Nicht blockierende Pipeline-Maschine: Quelle → Prozessorkette → Sinks
///        (ADR 0005). Kennt Komponenten ausschließlich über Locator-Interfaces
///        und IDs; initialisiert niemals Manager oder Komponenten (ADR 0004).
///
/// Lebenszyklus:
/// - begin(nowMs) validiert die Konfiguration, löst IDs zu gecachten Pointern
///   auf und ist nach Registry-Änderungen explizit erneut aufzurufen.
/// - update(nowMs) leistet pro Aufruf höchstens einen Zustandsübergang plus
///   die begrenzte Arbeit des aktuellen Zustands; blockiert nie.
/// - Fault ist sticky (nur behebbar durch erneutes begin()); Zyklusfehler
///   werden per RetryPolicy wiederholt und beenden die Pipeline nicht.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "PipelineTypes.h"

namespace mea {

class MeasurementPipelineMachine final {
public:
    /// Kapazitätsgrenzen je Pipeline (Compile-Time, ADR 0005).
    static constexpr std::size_t kMaxProcessors = 8;
    static constexpr std::size_t kMaxSinks = 4;

    /// Locator-Referenzen sind nicht besitzend und müssen die Maschine
    /// überleben (ADR 0001); implementiert werden sie von den Managern.
    MeasurementPipelineMachine(const ISourceLocator& sources,
                               const IProcessorLocator& processors,
                               const ISinkLocator& sinks,
                               const PipelineConfig& config) noexcept;

    /// Unkonfigurierte Maschine (für Runtime-Fassaden, die Pipelines in
    /// fester Kapazität vorhalten): vor begin() muss configure() erfolgen.
    MeasurementPipelineMachine() noexcept = default;

    /// Setzt Locators und Konfiguration nachträglich (reinitialisierend:
    /// die Maschine fällt auf Uninitialized zurück; Zähler laufen weiter).
    Status configure(const ISourceLocator& sources, const IProcessorLocator& processors,
                     const ISinkLocator& sinks, const PipelineConfig& config) noexcept;

    /// Validiert die Konfiguration und löst alle IDs auf. Reinitialisierend:
    /// bricht einen laufenden Zyklus ab; Zähler laufen weiter (ADR 0004).
    /// NotInitialized, wenn die Maschine noch nicht konfiguriert wurde.
    Status begin(TimestampMs nowMs) noexcept;

    /// Treibt die Maschine an; nicht blockierend, begrenzte Arbeit pro Aufruf.
    Status update(TimestampMs nowMs) noexcept;

    /// Aktiviert eine deaktivierte Pipeline; der nächste Zyklus ist sofort fällig.
    Status enable(TimestampMs nowMs) noexcept;

    /// Deaktiviert die Pipeline; ein laufender Zyklus wird verworfen.
    Status disable() noexcept;

    [[nodiscard]] ComponentId id() const noexcept { return config_.pipelineId; }
    [[nodiscard]] PipelineState state() const noexcept { return state_; }
    [[nodiscard]] Status lastStatus() const noexcept { return lastStatus_; }
    [[nodiscard]] std::uint32_t completedCycles() const noexcept {
        return completedCycles_;
    }
    [[nodiscard]] std::uint32_t failedCycles() const noexcept { return failedCycles_; }
    [[nodiscard]] std::uint32_t droppedMeasurements() const noexcept {
        return droppedMeasurements_;
    }
    /// Letzter vollständig verarbeiteter Messwert (vor der Sink-Übergabe).
    [[nodiscard]] const Measurement& lastMeasurement() const noexcept {
        return lastMeasurement_;
    }

private:
    enum class RetryStage : std::uint8_t { Acquire, Publish };

    [[nodiscard]] Status validateConfig() const noexcept;
    [[nodiscard]] Status resolveComponents() noexcept;
    [[nodiscard]] static bool isFatal(StatusCode code) noexcept;

    void startCycle(TimestampMs nowMs) noexcept;
    Status updateWaitingForCycle(TimestampMs nowMs) noexcept;
    Status updateWaitingForMeasurement(TimestampMs nowMs) noexcept;
    Status updateProcessing(TimestampMs nowMs) noexcept;
    Status updatePublishing(TimestampMs nowMs) noexcept;
    Status updateRetryDelay(TimestampMs nowMs) noexcept;

    /// Behandelt einen Fehler im laufenden Zyklus: fatal → Fault, sonst
    /// RetryPolicy anwenden bzw. Zyklus als fehlgeschlagen abschließen.
    Status failCycle(Status status, TimestampMs nowMs, RetryStage stage) noexcept;
    void abandonCycle() noexcept;

    // Locators als Zeiger, damit die Maschine default-konstruierbar bleibt;
    // nullptr = noch nicht konfiguriert (nicht besitzend, ADR 0001).
    const ISourceLocator* sources_{nullptr};
    const IProcessorLocator* processors_{nullptr};
    const ISinkLocator* sinks_{nullptr};
    PipelineConfig config_{};

    IMeasurementSource* source_{nullptr};
    IMeasurementProcessor* processors_cache_[kMaxProcessors]{};
    IMeasurementSink* sinks_cache_[kMaxSinks]{};

    Measurement working_{};
    Measurement lastMeasurement_{};
    PipelineState state_{PipelineState::Uninitialized};
    Status lastStatus_{StatusCode::NotInitialized, InvalidComponentId, 0};

    TimestampMs cycleStartMs_{0};
    TimestampMs acquisitionStartMs_{0};
    TimestampMs publishStartMs_{0};
    TimestampMs retryStartMs_{0};
    RetryStage retryStage_{RetryStage::Acquire};
    std::uint16_t attempts_{0};
    std::uint8_t acceptedSinkMask_{0};

    std::uint32_t completedCycles_{0};
    std::uint32_t failedCycles_{0};
    std::uint32_t droppedMeasurements_{0};
};

}  // namespace mea
