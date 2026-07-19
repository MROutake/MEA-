#include "Aht20Sensor.h"

namespace mea {

Aht20Sensor::Aht20Sensor(Aht20Device& device, const Config& config) noexcept
    : device_(device), config_(config) {}

ComponentId Aht20Sensor::id() const noexcept {
    return config_.sourceId;
}

Status Aht20Sensor::begin() noexcept {
    initialized_ = false;
    if (config_.sourceId == InvalidComponentId) {
        return makeStatus(StatusCode::InvalidConfiguration, config_.sourceId);
    }
    if (!device_.initialized()) {
        // Composition Root muss Aht20Device::begin() zuerst aufrufen (ADR 0004).
        return makeStatus(StatusCode::NotInitialized, config_.sourceId);
    }

    queue_.clear();
    lastSampleId_ = device_.sampleId();  // nur ab jetzt neue Samples übernehmen
    initialized_ = true;
    return okStatus();
}

Status Aht20Sensor::update(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, config_.sourceId);
    }

    Status status = device_.poll(nowMs);

    // Neues Sample unabhängig vom poll-Ergebnis übernehmen: der jeweils andere
    // Kanal kann die Messung bereits abgeschlossen haben.
    if (device_.sampleId() != lastSampleId_) {
        takeNewSample();
    }

    if (!status.ok() && status.origin == InvalidComponentId) {
        status.origin = config_.sourceId;
    }
    return status;
}

void Aht20Sensor::takeNewSample() noexcept {
    lastSampleId_ = device_.sampleId();
    ++sequence_;  // zählt auch verworfene Messwerte (Lücken zeigen Verluste an)

    const Aht20Sample& sample = device_.latestSample();
    Measurement measurement{};
    measurement.sourceId = config_.sourceId;
    if (config_.channel == Channel::Temperature) {
        measurement.kind = MeasurementKind::Temperature;
        measurement.unit = Unit::DegreeCelsius;
        measurement.value = sample.temperatureCelsius;
    } else {
        measurement.kind = MeasurementKind::Humidity;
        measurement.unit = Unit::Percent;
        measurement.value = sample.humidityPercent;
    }
    measurement.sampledAtMs = device_.sampledAtMs();  // Abschluss der Messung (ADR 0003)
    measurement.sequence = sequence_;
    measurement.quality = QualityFlag::None;

    if (!queue_.push(measurement)) {
        ++droppedMeasurements_;  // Drop-Policy: neuen Messwert verwerfen
    }
}

std::size_t Aht20Sensor::available() const noexcept {
    return queue_.size();
}

Status Aht20Sensor::read(Measurement& output) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, config_.sourceId);
    }
    if (!queue_.pop(output)) {
        return makeStatus(StatusCode::NoData, config_.sourceId);
    }
    return okStatus();
}

std::uint32_t Aht20Sensor::droppedMeasurements() const noexcept {
    return droppedMeasurements_;
}

}  // namespace mea
