#pragma once

#include <cstdint>

#include <MeaCore.h>
#include <MeaManagers.h>

#include "MeasurementPipelineMachine.h"

namespace mea {

class DefaultCycleOrchestrator final {
public:
    enum class Phase : std::uint8_t {
        AcquireSensors = 0,
        HandleCommands,
        RunPipeline,
        ApplyOutputs,
        Publish
    };

    struct Watchdogs {
        TimestampMs acquireSensorsMs{1000};
        TimestampMs handleCommandsMs{1000};
        TimestampMs runPipelineMs{1000};
        TimestampMs applyOutputsMs{1000};
        TimestampMs publishMs{1000};
    };

    struct Config {
        Watchdogs watchdogs{};
        bool skipPipelineIfNoData{true};
        bool continueAfterNonTransientError{true};
    };

    DefaultCycleOrchestrator(SensorManager<1>& sources, SinkManager<1>& sinks,
                             MeasurementPipelineMachine& pipeline,
                             IMeasurementSource& primarySource,
                             const Config& config,
                             const ComponentId origin = InvalidComponentId) noexcept
        : sources_(sources),
          sinks_(sinks),
          pipeline_(pipeline),
          primarySource_(primarySource),
          config_(config),
          origin_(origin) {}

    Status begin(const TimestampMs nowMs) noexcept {
        phase_ = Phase::AcquireSensors;
        phaseEnteredAtMs_ = nowMs;
        lastStatus_ = okStatus();
        initialized_ = true;
        return okStatus();
    }

    Status update(const TimestampMs nowMs) noexcept {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, origin_);
        }

        const Status status = executeCurrentPhase(nowMs);
        lastStatus_ = status;

        if (status.ok()) {
            advance(nowMs);
            return status;
        }

        if (status.transient()) {
            if (watchdogElapsed(nowMs)) {
                lastStatus_ = makeStatus(StatusCode::Timeout, origin_,
                                         static_cast<std::uint16_t>(phase_));
                advance(nowMs);
            }
            return status;
        }

        if (config_.continueAfterNonTransientError || watchdogElapsed(nowMs)) {
            advance(nowMs);
        }
        return status;
    }

    [[nodiscard]] Phase phase() const noexcept { return phase_; }
    [[nodiscard]] Status lastStatus() const noexcept { return lastStatus_; }

    [[nodiscard]] static const char* phaseName(const Phase phase) noexcept {
        switch (phase) {
            case Phase::AcquireSensors:
                return "AcquireSensors";
            case Phase::HandleCommands:
                return "HandleCommands";
            case Phase::RunPipeline:
                return "RunPipeline";
            case Phase::ApplyOutputs:
                return "ApplyOutputs";
            case Phase::Publish:
                return "Publish";
        }
        return "Unknown";
    }

private:
    [[nodiscard]] Status executeCurrentPhase(const TimestampMs nowMs) noexcept {
        switch (phase_) {
            case Phase::AcquireSensors:
                return sources_.updateAll(nowMs);
            case Phase::HandleCommands:
                return okStatus();
            case Phase::RunPipeline:
                if (config_.skipPipelineIfNoData &&
                    pipeline_.state() == PipelineState::WaitingForMeasurement &&
                    primarySource_.available() == 0U) {
                    return okStatus();
                }
                return pipeline_.update(nowMs);
            case Phase::ApplyOutputs:
                return sinks_.updateAll(nowMs);
            case Phase::Publish:
                return okStatus();
        }
        return makeStatus(StatusCode::InternalError, origin_);
    }

    void advance(const TimestampMs nowMs) noexcept {
        switch (phase_) {
            case Phase::AcquireSensors:
                phase_ = Phase::HandleCommands;
                break;
            case Phase::HandleCommands:
                phase_ = Phase::RunPipeline;
                break;
            case Phase::RunPipeline:
                phase_ = Phase::ApplyOutputs;
                break;
            case Phase::ApplyOutputs:
                phase_ = Phase::Publish;
                break;
            case Phase::Publish:
                phase_ = Phase::AcquireSensors;
                break;
        }
        phaseEnteredAtMs_ = nowMs;
    }

    [[nodiscard]] bool watchdogElapsed(const TimestampMs nowMs) const noexcept {
        const TimestampMs limit = watchdogFor(phase_);
        return limit > 0U && intervalElapsed(nowMs, phaseEnteredAtMs_, limit);
    }

    [[nodiscard]] TimestampMs watchdogFor(const Phase phase) const noexcept {
        switch (phase) {
            case Phase::AcquireSensors:
                return config_.watchdogs.acquireSensorsMs;
            case Phase::HandleCommands:
                return config_.watchdogs.handleCommandsMs;
            case Phase::RunPipeline:
                return config_.watchdogs.runPipelineMs;
            case Phase::ApplyOutputs:
                return config_.watchdogs.applyOutputsMs;
            case Phase::Publish:
                return config_.watchdogs.publishMs;
        }
        return 0;
    }

    SensorManager<1>& sources_;
    SinkManager<1>& sinks_;
    MeasurementPipelineMachine& pipeline_;
    IMeasurementSource& primarySource_;
    Config config_{};
    ComponentId origin_{InvalidComponentId};

    Phase phase_{Phase::AcquireSensors};
    TimestampMs phaseEnteredAtMs_{0};
    Status lastStatus_{};
    bool initialized_{false};
};

}  // namespace mea
