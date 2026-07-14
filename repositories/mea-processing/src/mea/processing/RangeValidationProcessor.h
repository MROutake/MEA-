#pragma once

/// @file RangeValidationProcessor.h
/// @brief Prüft Werte gegen [minValue, maxValue], ohne sie zu verändern.
///        Verletzungen werden über QualityFlag::OutOfRange gemeldet, der
///        Operationsstatus bleibt Ok (ADR 0003). Art/Einheit unverändert.

#include <cmath>

#include <MeaCore.h>

namespace mea {

class RangeValidationProcessor final : public IMeasurementProcessor {
public:
    struct Config {
        ComponentId processorId{InvalidComponentId};
        float minValue{0.0F};
        float maxValue{0.0F};
        /// Erwartete Eingabe; Unknown/None = Wildcard.
        MeasurementKind inputKind{MeasurementKind::Unknown};
        Unit inputUnit{Unit::None};
    };

    explicit RangeValidationProcessor(const Config& config) noexcept : config_(config) {}

    [[nodiscard]] ComponentId id() const noexcept override { return config_.processorId; }

    /// Reinitialisierend: erneuter Aufruf ist erlaubt und gibt Ok zurück (ADR 0004).
    Status begin() noexcept override {
        initialized_ = false;
        if (config_.processorId == InvalidComponentId ||
            !std::isfinite(config_.minValue) || !std::isfinite(config_.maxValue) ||
            config_.minValue > config_.maxValue) {
            return makeStatus(StatusCode::InvalidConfiguration, config_.processorId);
        }
        initialized_ = true;
        return okStatus();
    }

    [[nodiscard]] bool accepts(const MeasurementKind kind,
                               const Unit unit) const noexcept override {
        const bool kindOk =
            config_.inputKind == MeasurementKind::Unknown || kind == config_.inputKind;
        const bool unitOk = config_.inputUnit == Unit::None || unit == config_.inputUnit;
        return kindOk && unitOk;
    }

    Status process(const Measurement& input, Measurement& output) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, config_.processorId);
        }
        if (!accepts(input.kind, input.unit)) {
            return makeStatus(StatusCode::Unsupported, config_.processorId);
        }
        if (!hasFiniteValue(input)) {
            return makeStatus(StatusCode::InvalidArgument, config_.processorId);
        }

        output = input;
        if (input.value < config_.minValue || input.value > config_.maxValue) {
            output.quality |= QualityFlag::OutOfRange;
        }
        return okStatus();
    }

private:
    Config config_;
    bool initialized_{false};
};

}  // namespace mea
