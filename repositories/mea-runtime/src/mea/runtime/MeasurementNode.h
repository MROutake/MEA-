#pragma once

/// @file MeasurementNode.h
/// @brief Runtime-Fassade für einen Messknoten (ADR 0007): kapselt Manager,
///        Lebenszyklus-Reihenfolge, Pipeline-Bau und Fehlerreport hinter einer
///        deklarativen API. Der Composition Root besitzt weiterhin alle
///        Komponenten (ADR 0001); der Node hält nur nicht besitzende Zeiger.
///
/// Verwendung:
///
///     mea::MeasurementNode<8, 4, 2, 5, 4> node;
///     node.setReporter(&reportStatus);
///     node.setDefaultTuning({1000, 5000, 1000, {250, 3}, true});
///     node.addDevice(transport).addDevice(aht20Device);
///     node.addPipeline(ids::SoilPipeline, soilSensor)
///         .through(rawToPercent, clamp)
///         .into(serialSink);
///     node.begin(millis());   // Geräte -> Quellen -> Prozessoren/Sinks -> Pipelines
///     ...
///     node.update(millis());  // nicht blockierend, feste Reihenfolge
///
/// Fehlermodell:
/// - Verdrahtungsfehler (doppelte IDs, Kapazität) werden gesammelt und bei
///   begin() gemeldet: der Node startet dann nicht (Programmierfehler).
/// - Hardware-Ausfälle (Gerät/Quelle schlägt bei begin() fehl) deaktivieren
///   nur die betroffenen Pipelines; der Rest des Knotens läuft weiter.
/// - Prozessor-/Sink-/Pipeline-Konfigurationsfehler sind strikt.
/// - update() meldet schwere Fehler genau einmal je Statuswechsel über den
///   Reporter (kein Log-Sturm).

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>
#include <MeaManagers.h>
#include <MeaStateMachine.h>

namespace mea {

/// Zeitverhalten einer Pipeline; die Verdrahtung (IDs) übernimmt der Node.
struct PipelineTuning {
    TimestampMs cycleIntervalMs{1000};
    TimestampMs acquisitionTimeoutMs{1000};
    TimestampMs publishTimeoutMs{1000};
    RetryPolicy retry{};
    bool startImmediately{true};
};

/// Meldekanal für Statusfehler (z. B. auf Serial); nullptr = still.
using StatusReporter = void (*)(const char* stage, const Status& status);

template <std::size_t MaxSources = 8, std::size_t MaxProcessors = 8,
          std::size_t MaxSinks = 4, std::size_t MaxPipelines = 8,
          std::size_t MaxDevices = 4>
class MeasurementNode {
    static_assert(MaxSources > 0 && MaxSinks > 0 && MaxPipelines > 0,
                  "MeasurementNode benötigt Kapazität > 0");

public:
    /// Fluent-Verdrahtung einer Pipeline; Fehler werden im Node gesammelt
    /// und bei begin() gemeldet.
    class PipelineBuilder {
    public:
        /// Hängt Prozessoren in Ausführungsreihenfolge an.
        template <typename... Processors>
        PipelineBuilder& through(Processors&... processors) noexcept {
            (node_.addProcessorToSlot(slot_, processors), ...);
            return *this;
        }

        /// Fügt Sinks hinzu (mindestens einer ist Pflicht, ADR 0005).
        template <typename... Sinks>
        PipelineBuilder& into(Sinks&... sinks) noexcept {
            (node_.addSinkToSlot(slot_, sinks), ...);
            return *this;
        }

        /// Überschreibt das Default-Tuning dieser Pipeline.
        PipelineBuilder& tuned(const PipelineTuning& tuning) noexcept {
            node_.tuneSlot(slot_, tuning);
            return *this;
        }

    private:
        friend class MeasurementNode;
        PipelineBuilder(MeasurementNode& node, const std::size_t slot) noexcept
            : node_(node), slot_(slot) {}

        MeasurementNode& node_;
        std::size_t slot_;
    };

    void setReporter(const StatusReporter reporter) noexcept { reporter_ = reporter; }

    void setDefaultTuning(const PipelineTuning& tuning) noexcept {
        defaultTuning_ = tuning;
    }

    /// Registriert ein Gerät (geteilter Chip, Transport, ...). Geräte werden
    /// bei begin() vor allen Komponenten initialisiert und bei update()
    /// zuerst angetrieben (Registrierungsreihenfolge).
    MeasurementNode& addDevice(IDevice& device) noexcept {
        if (deviceCount_ >= MaxDevices) {
            noteBuildError(makeStatus(StatusCode::CapacityExceeded, InvalidComponentId));
            return *this;
        }
        for (std::size_t index = 0; index < deviceCount_; ++index) {
            if (devices_[index] == &device) {
                return *this;  // bereits registriert
            }
        }
        devices_[deviceCount_] = &device;
        ++deviceCount_;
        return *this;
    }

    /// Legt eine Pipeline mit genau einer Quelle an; die Quelle wird dabei
    /// automatisch registriert (mehrfache Verwendung derselben Quelle in
    /// weiteren Pipelines ist erlaubt).
    PipelineBuilder addPipeline(const ComponentId pipelineId,
                                IMeasurementSource& source) noexcept {
        if (pipelineId == InvalidComponentId) {
            noteBuildError(makeStatus(StatusCode::InvalidConfiguration, pipelineId));
            return PipelineBuilder(*this, kInvalidSlot);
        }
        if (pipelineCount_ >= MaxPipelines) {
            noteBuildError(makeStatus(StatusCode::CapacityExceeded, pipelineId));
            return PipelineBuilder(*this, kInvalidSlot);
        }
        for (std::size_t index = 0; index < pipelineCount_; ++index) {
            if (pipelines_[index].pipelineId == pipelineId) {
                noteBuildError(makeStatus(StatusCode::DuplicateId, pipelineId));
                return PipelineBuilder(*this, kInvalidSlot);
            }
        }
        noteBuildError(registerOnce(sources_, source));

        PipelineSlot& slot = pipelines_[pipelineCount_];
        slot.pipelineId = pipelineId;
        slot.sourceId = source.id();
        return PipelineBuilder(*this, pipelineCount_++);
    }

    /// Initialisiert in fester Reihenfolge: Geräte -> Quellen -> Prozessoren
    /// -> Sinks -> Pipelines. Hardware-Ausfälle (Geräte/Quellen) werden
    /// gemeldet und deaktivieren nur die abhängigen Pipelines; alle übrigen
    /// Fehler brechen ab. Reinitialisierend (ADR 0004).
    Status begin(const TimestampMs nowMs) noexcept {
        begun_ = false;
        lastReportedCode_ = StatusCode::Ok;
        if (!buildError_.ok()) {
            report("node build", buildError_);
            return buildError_;
        }

        for (std::size_t index = 0; index < deviceCount_; ++index) {
            const Status status = devices_[index]->begin();
            deviceOk_[index] = status.ok();
            if (!status.ok()) {
                report("device begin", status);
            }
        }

        for (std::size_t index = 0; index < sources_.size(); ++index) {
            IMeasurementSource* const source = sources_.at(index);
            Status status = source->begin();
            if (!status.ok() && status.origin == InvalidComponentId) {
                status.origin = source->id();
            }
            sourceOk_[index] = status.ok();
            if (!status.ok()) {
                report("source begin", status);
            }
        }

        Status firstError = okStatus();
        for (std::size_t index = 0; index < processors_.size(); ++index) {
            IMeasurementProcessor* const processor = processors_.at(index);
            Status status = processor->begin();
            if (!status.ok()) {
                if (status.origin == InvalidComponentId) {
                    status.origin = processor->id();
                }
                report("processor begin", status);
                if (firstError.ok()) {
                    firstError = status;
                }
            }
        }
        for (std::size_t index = 0; index < sinks_.size(); ++index) {
            IMeasurementSink* const sink = sinks_.at(index);
            Status status = sink->begin();
            if (!status.ok()) {
                if (status.origin == InvalidComponentId) {
                    status.origin = sink->id();
                }
                report("sink begin", status);
                if (firstError.ok()) {
                    firstError = status;
                }
            }
        }

        for (std::size_t index = 0; index < pipelineCount_; ++index) {
            PipelineSlot& slot = pipelines_[index];
            if (!sourceAvailable(slot.sourceId)) {
                slot.active = false;  // Quelle ausgefallen: Pipeline auslassen
                continue;
            }
            const PipelineTuning& tuning =
                slot.useDefaultTuning ? defaultTuning_ : slot.tuning;
            PipelineConfig pipelineCfg{};
            pipelineCfg.pipelineId = slot.pipelineId;
            pipelineCfg.sourceId = slot.sourceId;
            pipelineCfg.processorIds =
                ArrayView<const ComponentId>(slot.processorIds, slot.processorCount);
            pipelineCfg.sinkIds = ArrayView<const ComponentId>(slot.sinkIds, slot.sinkCount);
            pipelineCfg.cycleIntervalMs = tuning.cycleIntervalMs;
            pipelineCfg.acquisitionTimeoutMs = tuning.acquisitionTimeoutMs;
            pipelineCfg.publishTimeoutMs = tuning.publishTimeoutMs;
            pipelineCfg.retry = tuning.retry;
            pipelineCfg.startImmediately = tuning.startImmediately;

            (void)slot.machine.configure(sources_, processors_, sinks_, pipelineCfg);
            const Status status = slot.machine.begin(nowMs);
            slot.active = status.ok();
            if (!status.ok()) {
                report("pipeline begin", status);
                if (firstError.ok()) {
                    firstError = status;
                }
            }
        }

        begun_ = firstError.ok();
        return firstError;
    }

    /// Nicht blockierender Antrieb in fester Reihenfolge: Geräte -> Quellen
    /// -> Sinks -> Pipelines. Liefert den ersten schweren Fehler des Ticks
    /// und meldet ihn genau einmal je Statuswechsel über den Reporter.
    Status update(const TimestampMs nowMs) noexcept {
        if (!begun_) {
            return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
        }

        Status severe = okStatus();
        for (std::size_t index = 0; index < deviceCount_; ++index) {
            if (deviceOk_[index]) {
                noteSevere(severe, devices_[index]->update(nowMs));
            }
        }
        for (std::size_t index = 0; index < sources_.size(); ++index) {
            if (sourceOk_[index]) {
                Status status = sources_.at(index)->update(nowMs);
                if (!status.ok() && status.origin == InvalidComponentId) {
                    status.origin = sources_.at(index)->id();
                }
                noteSevere(severe, status);
            }
        }
        for (std::size_t index = 0; index < sinks_.size(); ++index) {
            Status status = sinks_.at(index)->update(nowMs);
            if (!status.ok() && status.origin == InvalidComponentId) {
                status.origin = sinks_.at(index)->id();
            }
            noteSevere(severe, status);
        }
        for (std::size_t index = 0; index < pipelineCount_; ++index) {
            if (pipelines_[index].active) {
                noteSevere(severe, pipelines_[index].machine.update(nowMs));
            }
        }

        if (!severe.ok()) {
            if (severe.code != lastReportedCode_) {
                lastReportedCode_ = severe.code;
                report("update", severe);
            }
        } else {
            lastReportedCode_ = StatusCode::Ok;
        }
        return severe;
    }

    /// Erster gesammelter Verdrahtungsfehler (Ok, wenn der Aufbau gültig ist).
    [[nodiscard]] Status buildError() const noexcept { return buildError_; }

    [[nodiscard]] std::size_t pipelineCount() const noexcept { return pipelineCount_; }

    [[nodiscard]] std::size_t activePipelines() const noexcept {
        std::size_t count = 0;
        for (std::size_t index = 0; index < pipelineCount_; ++index) {
            if (pipelines_[index].active) {
                ++count;
            }
        }
        return count;
    }

    /// Diagnosezugriff auf eine Pipeline; nullptr bei unbekannter ID.
    [[nodiscard]] const MeasurementPipelineMachine* pipeline(
        const ComponentId pipelineId) const noexcept {
        for (std::size_t index = 0; index < pipelineCount_; ++index) {
            if (pipelines_[index].pipelineId == pipelineId) {
                return &pipelines_[index].machine;
            }
        }
        return nullptr;
    }

private:
    static constexpr std::size_t kInvalidSlot = MaxPipelines;

    struct PipelineSlot {
        MeasurementPipelineMachine machine{};
        PipelineTuning tuning{};
        ComponentId pipelineId{InvalidComponentId};
        ComponentId sourceId{InvalidComponentId};
        ComponentId processorIds[MeasurementPipelineMachine::kMaxProcessors]{};
        std::size_t processorCount{0};
        ComponentId sinkIds[MeasurementPipelineMachine::kMaxSinks]{};
        std::size_t sinkCount{0};
        bool useDefaultTuning{true};
        bool active{false};
    };

    friend class PipelineBuilder;

    /// Registriert eine Komponente genau einmal; dasselbe Objekt darf mehrfach
    /// verwendet werden, eine fremde Komponente mit gleicher ID ist ein Fehler.
    template <typename Manager, typename Component>
    [[nodiscard]] Status registerOnce(Manager& manager, Component& component) noexcept {
        auto* const existing = manager.find(component.id());
        if (existing == &component) {
            return okStatus();
        }
        if (existing != nullptr) {
            return makeStatus(StatusCode::DuplicateId, component.id());
        }
        return manager.registerComponent(component);
    }

    void addProcessorToSlot(const std::size_t slot,
                            IMeasurementProcessor& processor) noexcept {
        if (slot >= pipelineCount_) {
            return;  // Fehler wurde bereits beim Anlegen gesammelt
        }
        PipelineSlot& pipelineSlot = pipelines_[slot];
        if (pipelineSlot.processorCount >= MeasurementPipelineMachine::kMaxProcessors) {
            noteBuildError(
                makeStatus(StatusCode::CapacityExceeded, pipelineSlot.pipelineId));
            return;
        }
        noteBuildError(registerOnce(processors_, processor));
        pipelineSlot.processorIds[pipelineSlot.processorCount] = processor.id();
        ++pipelineSlot.processorCount;
    }

    void addSinkToSlot(const std::size_t slot, IMeasurementSink& sink) noexcept {
        if (slot >= pipelineCount_) {
            return;
        }
        PipelineSlot& pipelineSlot = pipelines_[slot];
        if (pipelineSlot.sinkCount >= MeasurementPipelineMachine::kMaxSinks) {
            noteBuildError(
                makeStatus(StatusCode::CapacityExceeded, pipelineSlot.pipelineId));
            return;
        }
        noteBuildError(registerOnce(sinks_, sink));
        pipelineSlot.sinkIds[pipelineSlot.sinkCount] = sink.id();
        ++pipelineSlot.sinkCount;
    }

    void tuneSlot(const std::size_t slot, const PipelineTuning& tuning) noexcept {
        if (slot >= pipelineCount_) {
            return;
        }
        pipelines_[slot].tuning = tuning;
        pipelines_[slot].useDefaultTuning = false;
    }

    [[nodiscard]] bool sourceAvailable(const ComponentId sourceId) const noexcept {
        for (std::size_t index = 0; index < sources_.size(); ++index) {
            if (sources_.at(index)->id() == sourceId) {
                return sourceOk_[index];
            }
        }
        return false;
    }

    void noteBuildError(const Status& status) noexcept {
        if (buildError_.ok() && !status.ok()) {
            buildError_ = status;
        }
    }

    static void noteSevere(Status& severe, const Status& status) noexcept {
        if (!status.ok() && !status.transient() && severe.ok()) {
            severe = status;
        }
    }

    void report(const char* stage, const Status& status) noexcept {
        if (reporter_ != nullptr) {
            reporter_(stage, status);
        }
    }

    // Manager dienen als Registries/Locators für die Pipelines (ADR 0005);
    // den Lebenszyklus treibt der Node selbst (je Komponente, degradierend).
    SensorManager<MaxSources> sources_{};
    ProcessorManager<MaxProcessors> processors_{};
    SinkManager<MaxSinks> sinks_{};

    IDevice* devices_[MaxDevices]{};
    std::size_t deviceCount_{0};
    bool deviceOk_[MaxDevices]{};
    bool sourceOk_[MaxSources]{};

    PipelineSlot pipelines_[MaxPipelines]{};
    std::size_t pipelineCount_{0};

    PipelineTuning defaultTuning_{};
    StatusReporter reporter_{nullptr};
    Status buildError_{};
    StatusCode lastReportedCode_{StatusCode::Ok};
    bool begun_{false};
};

}  // namespace mea
