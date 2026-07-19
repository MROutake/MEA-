#include "Aht20Device.h"

namespace mea {

Aht20Device::Aht20Device(IAht20Driver& driver, const Config& config) noexcept
    : driver_(driver), config_(config) {}

Status Aht20Device::begin() noexcept {
    initialized_ = false;
    if (config_.sampleIntervalMs == 0 || config_.measurementTimeoutMs == 0) {
        return makeStatus(StatusCode::InvalidConfiguration, InvalidComponentId);
    }

    const Status status = driver_.begin();
    if (!status.ok()) {
        return status;
    }

    measuring_ = false;
    firstCycle_ = true;
    initialized_ = true;
    return okStatus();
}

Status Aht20Device::poll(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
    }

    if (!measuring_) {
        const bool due =
            firstCycle_ || intervalElapsed(nowMs, cycleStartMs_, config_.sampleIntervalMs);
        if (!due) {
            return okStatus();
        }
        // Auch ein Fehlversuch startet erst nach dem Intervall erneut.
        firstCycle_ = false;
        cycleStartMs_ = nowMs;

        const Status status = driver_.triggerMeasurement();
        if (!status.ok()) {
            ++failedAcquisitions_;
            return status;
        }
        measuring_ = true;
        return okStatus();
    }

    Aht20Sample sample{};
    const Status status = driver_.readSample(sample);
    if (status.ok()) {
        measuring_ = false;
        sample_ = sample;
        sampledAtMs_ = nowMs;
        ++sampleId_;
        if (sampleId_ == 0) {
            sampleId_ = 1;  // 0 bleibt für "kein Sample" reserviert
        }
        return okStatus();
    }

    if (status.code == StatusCode::Busy) {
        if (intervalElapsed(nowMs, cycleStartMs_, config_.measurementTimeoutMs)) {
            measuring_ = false;
            ++failedAcquisitions_;
            return makeStatus(StatusCode::Timeout, InvalidComponentId);
        }
        return okStatus();  // Messung läuft noch
    }

    measuring_ = false;
    ++failedAcquisitions_;
    return status;
}

}  // namespace mea
