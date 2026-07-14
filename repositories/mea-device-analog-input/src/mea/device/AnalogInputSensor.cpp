#include "AnalogInputSensor.h"

#include <cstdint>
#include <limits>

namespace mea {

AnalogInputSensor::AnalogInputSensor(IAnalogReader& reader, const Config& config) noexcept
    : reader_(reader), config_(config) {}

ComponentId AnalogInputSensor::id() const noexcept {
    return config_.sourceId;
}

Status AnalogInputSensor::begin() noexcept {
    initialized_ = false;
    if (config_.sourceId == InvalidComponentId || config_.sampleIntervalMs == 0 ||
        config_.samplesPerMeasurement == 0 || config_.maxSamplesPerUpdate == 0) {
        return makeStatus(StatusCode::InvalidConfiguration, config_.sourceId);
    }

    Status status = reader_.beginPin(config_.pin);
    if (!status.ok()) {
        if (status.origin == InvalidComponentId) {
            status.origin = config_.sourceId;
        }
        return status;
    }

    queue_.clear();
    acquiring_ = false;
    firstCycle_ = true;
    samplesTaken_ = 0;
    sampleSum_ = 0;
    initialized_ = true;
    return okStatus();
}

Status AnalogInputSensor::update(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, config_.sourceId);
    }

    if (!acquiring_) {
        const bool due = firstCycle_ ||
                         intervalElapsed(nowMs, cycleStartMs_, config_.sampleIntervalMs);
        if (!due) {
            return okStatus();
        }
        acquiring_ = true;
        firstCycle_ = false;
        cycleStartMs_ = nowMs;
        sampleSum_ = 0;
        samplesTaken_ = 0;
    }

    return takeSamples(nowMs);
}

Status AnalogInputSensor::takeSamples(const TimestampMs nowMs) noexcept {
    for (std::uint8_t taken = 0; taken < config_.maxSamplesPerUpdate &&
                                 samplesTaken_ < config_.samplesPerMeasurement;
         ++taken) {
        std::uint32_t raw = 0;
        Status status = reader_.readRaw(config_.pin, raw);
        if (!status.ok()) {
            acquiring_ = false;
            ++failedAcquisitions_;
            if (status.origin == InvalidComponentId) {
                status.origin = config_.sourceId;
            }
            return status;
        }

        // Summenüberlauf abfangen (z. B. viele Samples bei großem Rohbereich).
        if (raw > std::numeric_limits<std::uint32_t>::max() - sampleSum_) {
            acquiring_ = false;
            ++failedAcquisitions_;
            return makeStatus(StatusCode::ProcessingError, config_.sourceId, 1);
        }

        sampleSum_ += raw;
        ++samplesTaken_;
    }

    if (samplesTaken_ == config_.samplesPerMeasurement) {
        finishMeasurement(nowMs);
    }
    return okStatus();
}

void AnalogInputSensor::finishMeasurement(const TimestampMs nowMs) noexcept {
    acquiring_ = false;
    ++sequence_;  // zählt auch verworfene Messwerte (Lücken zeigen Verluste an)

    Measurement measurement{};
    measurement.sourceId = config_.sourceId;
    measurement.kind = config_.outputKind;
    measurement.unit = config_.outputUnit;
    measurement.value = static_cast<float>(sampleSum_) /
                        static_cast<float>(config_.samplesPerMeasurement);
    measurement.sampledAtMs = nowMs;  // Abschluss der Messung (ADR 0003)
    measurement.sequence = sequence_;
    measurement.quality = QualityFlag::None;

    if (!queue_.push(measurement)) {
        ++droppedMeasurements_;  // Drop-Policy: neuen Messwert verwerfen
    }
}

std::size_t AnalogInputSensor::available() const noexcept {
    return queue_.size();
}

Status AnalogInputSensor::read(Measurement& output) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, config_.sourceId);
    }
    if (!queue_.pop(output)) {
        return makeStatus(StatusCode::NoData, config_.sourceId);
    }
    return okStatus();
}

std::uint32_t AnalogInputSensor::droppedMeasurements() const noexcept {
    return droppedMeasurements_;
}

std::uint32_t AnalogInputSensor::failedAcquisitions() const noexcept {
    return failedAcquisitions_;
}

}  // namespace mea
