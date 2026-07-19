#include "Bmp280Device.h"

namespace mea {

Bmp280Device::Bmp280Device(IBmp280Driver& driver, const Config& config) noexcept
    : driver_(driver), config_(config) {}

Status Bmp280Device::begin() noexcept {
    initialized_ = false;
    if (config_.sampleIntervalMs == 0) {
        return makeStatus(StatusCode::InvalidConfiguration, InvalidComponentId);
    }

    const Status status = driver_.begin();
    if (!status.ok()) {
        return status;
    }

    firstCycle_ = true;
    initialized_ = true;
    return okStatus();
}

Status Bmp280Device::poll(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
    }

    const bool due =
        firstCycle_ || intervalElapsed(nowMs, cycleStartMs_, config_.sampleIntervalMs);
    if (!due) {
        return okStatus();
    }
    firstCycle_ = false;
    cycleStartMs_ = nowMs;

    Bmp280Sample sample{};
    const Status status = driver_.readSample(sample);
    if (status.code == StatusCode::Busy) {
        // Erste Messung nach begin() läuft noch: beim nächsten poll() erneut.
        firstCycle_ = true;
        return okStatus();
    }
    if (!status.ok()) {
        // Auch ein Fehlversuch startet erst nach dem Intervall erneut.
        ++failedAcquisitions_;
        return status;
    }

    sample_ = sample;
    sampledAtMs_ = nowMs;
    ++sampleId_;
    if (sampleId_ == 0) {
        sampleId_ = 1;  // 0 bleibt für "kein Sample" reserviert
    }
    return okStatus();
}

}  // namespace mea
