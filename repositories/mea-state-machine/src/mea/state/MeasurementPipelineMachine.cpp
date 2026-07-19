#include "MeasurementPipelineMachine.h"

namespace mea {

MeasurementPipelineMachine::MeasurementPipelineMachine(
    const ISourceLocator& sources, const IProcessorLocator& processors,
    const ISinkLocator& sinks, const PipelineConfig& config) noexcept
    : sources_(&sources), processors_(&processors), sinks_(&sinks), config_(config) {}

Status MeasurementPipelineMachine::configure(const ISourceLocator& sources,
                                             const IProcessorLocator& processors,
                                             const ISinkLocator& sinks,
                                             const PipelineConfig& config) noexcept {
    sources_ = &sources;
    processors_ = &processors;
    sinks_ = &sinks;
    config_ = config;
    source_ = nullptr;
    abandonCycle();
    state_ = PipelineState::Uninitialized;
    lastStatus_ = Status{StatusCode::NotInitialized, InvalidComponentId, 0};
    return okStatus();
}

// ------------------------------------------------------------ Lebenszyklus

Status MeasurementPipelineMachine::validateConfig() const noexcept {
    const ComponentId pipelineId = config_.pipelineId;
    if (pipelineId == InvalidComponentId || config_.sourceId == InvalidComponentId ||
        config_.cycleIntervalMs == 0 || config_.acquisitionTimeoutMs == 0 ||
        config_.publishTimeoutMs == 0) {
        return makeStatus(StatusCode::InvalidConfiguration, pipelineId);
    }
    if (config_.sinkIds.empty() ||
        (config_.sinkIds.data() == nullptr && !config_.sinkIds.empty()) ||
        (config_.processorIds.data() == nullptr && !config_.processorIds.empty())) {
        return makeStatus(StatusCode::InvalidConfiguration, pipelineId);
    }
    if (config_.processorIds.size() > kMaxProcessors ||
        config_.sinkIds.size() > kMaxSinks) {
        return makeStatus(StatusCode::CapacityExceeded, pipelineId);
    }
    for (std::size_t index = 0; index < config_.processorIds.size(); ++index) {
        if (config_.processorIds[index] == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidConfiguration, pipelineId);
        }
        for (std::size_t other = index + 1; other < config_.processorIds.size();
             ++other) {
            if (config_.processorIds[index] == config_.processorIds[other]) {
                return makeStatus(StatusCode::DuplicateId, pipelineId,
                                  config_.processorIds[index]);
            }
        }
    }
    for (std::size_t index = 0; index < config_.sinkIds.size(); ++index) {
        if (config_.sinkIds[index] == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidConfiguration, pipelineId);
        }
        for (std::size_t other = index + 1; other < config_.sinkIds.size(); ++other) {
            if (config_.sinkIds[index] == config_.sinkIds[other]) {
                return makeStatus(StatusCode::DuplicateId, pipelineId,
                                  config_.sinkIds[index]);
            }
        }
    }
    return okStatus();
}

Status MeasurementPipelineMachine::resolveComponents() noexcept {
    source_ = sources_->find(config_.sourceId);
    if (source_ == nullptr) {
        return makeStatus(StatusCode::NotFound, config_.pipelineId, config_.sourceId);
    }
    for (std::size_t index = 0; index < config_.processorIds.size(); ++index) {
        processors_cache_[index] = processors_->find(config_.processorIds[index]);
        if (processors_cache_[index] == nullptr) {
            return makeStatus(StatusCode::NotFound, config_.pipelineId,
                              config_.processorIds[index]);
        }
    }
    for (std::size_t index = 0; index < config_.sinkIds.size(); ++index) {
        sinks_cache_[index] = sinks_->find(config_.sinkIds[index]);
        if (sinks_cache_[index] == nullptr) {
            return makeStatus(StatusCode::NotFound, config_.pipelineId,
                              config_.sinkIds[index]);
        }
    }
    return okStatus();
}

Status MeasurementPipelineMachine::begin(const TimestampMs nowMs) noexcept {
    if (sources_ == nullptr || processors_ == nullptr || sinks_ == nullptr) {
        // Default-konstruiert und nie konfiguriert (kein Fault: configure()
        // plus erneutes begin() genügt).
        return makeStatus(StatusCode::NotInitialized, config_.pipelineId);
    }
    source_ = nullptr;

    Status status = validateConfig();
    if (status.ok()) {
        status = resolveComponents();
    }
    if (!status.ok()) {
        state_ = PipelineState::Fault;
        lastStatus_ = status;
        return status;
    }

    // Bewusst KEIN beginAll() der Manager (ADR 0004): Initialisierung der
    // Komponenten ist Aufgabe des Composition Roots.
    abandonCycle();
    lastStatus_ = okStatus();
    if (config_.startImmediately) {
        cycleStartMs_ = static_cast<TimestampMs>(nowMs - config_.cycleIntervalMs);
        state_ = PipelineState::WaitingForCycle;
    } else {
        state_ = PipelineState::Disabled;
    }
    return okStatus();
}

Status MeasurementPipelineMachine::enable(const TimestampMs nowMs) noexcept {
    if (state_ == PipelineState::Uninitialized) {
        return makeStatus(StatusCode::NotInitialized, config_.pipelineId);
    }
    if (state_ == PipelineState::Fault) {
        return lastStatus_;  // Fault erfordert explizites begin()
    }
    abandonCycle();
    cycleStartMs_ = static_cast<TimestampMs>(nowMs - config_.cycleIntervalMs);
    state_ = PipelineState::WaitingForCycle;
    return okStatus();
}

Status MeasurementPipelineMachine::disable() noexcept {
    if (state_ == PipelineState::Uninitialized) {
        return makeStatus(StatusCode::NotInitialized, config_.pipelineId);
    }
    if (state_ == PipelineState::Fault) {
        return lastStatus_;
    }
    abandonCycle();
    state_ = PipelineState::Disabled;
    return okStatus();
}

void MeasurementPipelineMachine::abandonCycle() noexcept {
    attempts_ = 0;
    acceptedSinkMask_ = 0;
}

// ------------------------------------------------------------ update()

Status MeasurementPipelineMachine::update(const TimestampMs nowMs) noexcept {
    switch (state_) {
        case PipelineState::Uninitialized:
            return makeStatus(StatusCode::NotInitialized, config_.pipelineId);
        case PipelineState::Disabled:
            return okStatus();
        case PipelineState::Fault:
            return lastStatus_;
        case PipelineState::WaitingForCycle:
            return updateWaitingForCycle(nowMs);
        case PipelineState::WaitingForMeasurement:
            return updateWaitingForMeasurement(nowMs);
        case PipelineState::Processing:
            return updateProcessing(nowMs);
        case PipelineState::Publishing:
        case PipelineState::Backpressure:
            return updatePublishing(nowMs);
        case PipelineState::RetryDelay:
            return updateRetryDelay(nowMs);
    }
    return makeStatus(StatusCode::InternalError, config_.pipelineId);
}

void MeasurementPipelineMachine::startCycle(const TimestampMs nowMs) noexcept {
    cycleStartMs_ = nowMs;
    acquisitionStartMs_ = nowMs;
    attempts_ = 0;
    acceptedSinkMask_ = 0;
    state_ = PipelineState::WaitingForMeasurement;
}

Status MeasurementPipelineMachine::updateWaitingForCycle(
    const TimestampMs nowMs) noexcept {
    if (intervalElapsed(nowMs, cycleStartMs_, config_.cycleIntervalMs)) {
        startCycle(nowMs);
    }
    return okStatus();
}

Status MeasurementPipelineMachine::updateWaitingForMeasurement(
    const TimestampMs nowMs) noexcept {
    if (source_->available() > 0) {
        const Status status = source_->read(working_);
        if (status.ok()) {
            state_ = PipelineState::Processing;
            return okStatus();
        }
        if (!status.transient()) {
            return failCycle(status, nowMs, RetryStage::Acquire);
        }
        // Transient (Race): weiter warten, Timeout läuft.
    }
    if (intervalElapsed(nowMs, acquisitionStartMs_, config_.acquisitionTimeoutMs)) {
        return failCycle(
            makeStatus(StatusCode::Timeout, config_.pipelineId, config_.sourceId), nowMs,
            RetryStage::Acquire);
    }
    return okStatus();
}

Status MeasurementPipelineMachine::updateProcessing(const TimestampMs nowMs) noexcept {
    // Die Kettenlänge ist konfigurationsbegrenzt (<= kMaxProcessors): die
    // gesamte Kette läuft in einem update() (ADR 0005).
    for (std::size_t index = 0; index < config_.processorIds.size(); ++index) {
        Measurement output{};
        const Status status = processors_cache_[index]->process(working_, output);
        if (!status.ok()) {
            return failCycle(status, nowMs, RetryStage::Acquire);
        }
        working_ = output;
    }
    lastMeasurement_ = working_;
    acceptedSinkMask_ = 0;
    publishStartMs_ = nowMs;
    state_ = PipelineState::Publishing;
    return okStatus();
}

Status MeasurementPipelineMachine::updatePublishing(const TimestampMs nowMs) noexcept {
    bool backpressure = false;

    for (std::size_t index = 0; index < config_.sinkIds.size(); ++index) {
        const auto sinkBit = static_cast<std::uint8_t>(1U << index);
        if ((acceptedSinkMask_ & sinkBit) != 0) {
            continue;  // dieser Sink hat den Wert bereits übernommen
        }
        const Status status = sinks_cache_[index]->submit(working_);
        if (status.ok()) {
            acceptedSinkMask_ = static_cast<std::uint8_t>(acceptedSinkMask_ | sinkBit);
            continue;
        }
        if (status.transient()) {
            backpressure = true;  // blockiert nicht die übrigen Sinks
            continue;
        }
        return failCycle(status, nowMs, RetryStage::Publish);
    }

    const auto allMask = static_cast<std::uint8_t>((1U << config_.sinkIds.size()) - 1U);
    if (acceptedSinkMask_ == allMask) {
        ++completedCycles_;
        lastStatus_ = okStatus();
        state_ = PipelineState::WaitingForCycle;
        return okStatus();
    }

    if (intervalElapsed(nowMs, publishStartMs_, config_.publishTimeoutMs)) {
        return failCycle(makeStatus(StatusCode::Timeout, config_.pipelineId), nowMs,
                         RetryStage::Publish);
    }

    state_ = backpressure ? PipelineState::Backpressure : PipelineState::Publishing;
    return okStatus();
}

Status MeasurementPipelineMachine::updateRetryDelay(const TimestampMs nowMs) noexcept {
    if (!intervalElapsed(nowMs, retryStartMs_, config_.retry.delayMs)) {
        return okStatus();
    }
    if (retryStage_ == RetryStage::Publish) {
        publishStartMs_ = nowMs;  // bereits übernommene Sinks bleiben markiert
        state_ = PipelineState::Publishing;
    } else {
        acquisitionStartMs_ = nowMs;
        state_ = PipelineState::WaitingForMeasurement;
    }
    return okStatus();
}

// ------------------------------------------------------------ Fehlerpfad

bool MeasurementPipelineMachine::isFatal(const StatusCode code) noexcept {
    return code == StatusCode::NotInitialized ||
           code == StatusCode::InvalidConfiguration || code == StatusCode::NotFound ||
           code == StatusCode::InternalError;
}

Status MeasurementPipelineMachine::failCycle(const Status status, const TimestampMs nowMs,
                                             const RetryStage stage) noexcept {
    lastStatus_ = status;

    if (isFatal(status.code)) {
        ++failedCycles_;
        state_ = PipelineState::Fault;
        return status;
    }

    if (attempts_ < config_.retry.maximumAttempts) {
        ++attempts_;
        retryStage_ = stage;
        retryStartMs_ = nowMs;
        state_ = PipelineState::RetryDelay;
        return status;
    }

    // Wiederholungen erschöpft: Zyklus abschreiben, Pipeline läuft weiter.
    ++failedCycles_;
    if (stage == RetryStage::Publish) {
        ++droppedMeasurements_;  // Messwert konnte nicht (vollständig) verteilt werden
    }
    abandonCycle();
    cycleStartMs_ = nowMs;
    state_ = PipelineState::WaitingForCycle;
    return status;
}

}  // namespace mea
