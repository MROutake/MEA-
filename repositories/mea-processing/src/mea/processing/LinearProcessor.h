#pragma once

/// @file LinearProcessor.h
/// @brief Lineare Umrechnung value' = gain * value + offset mit expliziter
///        Ausgabe-Art/-Einheit. Es gibt keine stillen Einheitenkonvertierungen:
///        Die Konfiguration benennt Eingangs- und Ausgangsart ausdrücklich.

#include <cmath>

#include <MeaCore.h>

namespace mea {

class LinearProcessor final : public IMeasurementProcessor {
public:
    struct Config {
        ComponentId processorId{InvalidComponentId};
        float gain{1.0F};
        float offset{0.0F};
        /// Erwartete Eingabe. MeasurementKind::Unknown bzw. Unit::None wirken
        /// als Wildcard (jede Art/Einheit wird akzeptiert).
        MeasurementKind inputKind{MeasurementKind::Unknown};
        Unit inputUnit{Unit::None};
        /// Art/Einheit des Ergebnisses (Pflicht, keine stille Konvertierung).
        MeasurementKind outputKind{MeasurementKind::Unknown};
        Unit outputUnit{Unit::None};
    };

    explicit LinearProcessor(const Config& config) noexcept : config_(config) {}

    [[nodiscard]] ComponentId id() const noexcept override { return config_.processorId; }

    /// Reinitialisierend: erneuter Aufruf ist erlaubt und gibt Ok zurück (ADR 0004).
    Status begin() noexcept override {
        initialized_ = false;
        if (config_.processorId == InvalidComponentId || !std::isfinite(config_.gain) ||
            !std::isfinite(config_.offset)) {
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

        const float result = (input.value * config_.gain) + config_.offset;
        if (!std::isfinite(result)) {
            return makeStatus(StatusCode::ProcessingError, config_.processorId);
        }

        output = input;
        output.value = result;
        output.kind = config_.outputKind;
        output.unit = config_.outputUnit;
        return okStatus();
    }

private:
    Config config_;
    bool initialized_{false};
};

}  // namespace mea
