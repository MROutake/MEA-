#pragma once

/// @file MovingAverageProcessor.h
/// @brief Gleitender Mittelwert über die letzten WindowSize Werte.
///        In der Anlaufphase (Fenster noch nicht voll) wird über die bislang
///        gesehenen Werte gemittelt und QualityFlag::Estimated gesetzt.
///        Zeitstempel und Sequenznummer der Ausgabe entsprechen der Eingabe.

#include <cmath>
#include <cstddef>

#include <MeaCore.h>

namespace mea {

/// @tparam WindowSize Compile-Time-Fenstergröße (> 0), bestimmt den RAM-Bedarf.
template <std::size_t WindowSize>
class MovingAverageProcessor final : public IMeasurementProcessor {
    static_assert(WindowSize > 0, "WindowSize muss > 0 sein");

public:
    struct Config {
        ComponentId processorId{InvalidComponentId};
        /// Erwartete Eingabe; Unknown/None = Wildcard.
        MeasurementKind inputKind{MeasurementKind::Unknown};
        Unit inputUnit{Unit::None};
    };

    explicit MovingAverageProcessor(const Config& config) noexcept : config_(config) {}

    [[nodiscard]] ComponentId id() const noexcept override { return config_.processorId; }

    /// Reinitialisierend: erneuter Aufruf leert das Fenster und gibt Ok zurück
    /// (ADR 0004).
    Status begin() noexcept override {
        initialized_ = false;
        if (config_.processorId == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidConfiguration, config_.processorId);
        }
        reset();
        initialized_ = true;
        return okStatus();
    }

    /// Leert das Fenster (nächste Ausgaben starten wieder in der Anlaufphase).
    void reset() noexcept {
        filled_ = 0;
        writeIndex_ = 0;
        sum_ = 0.0F;
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

        if (filled_ == WindowSize) {
            sum_ -= window_[writeIndex_];
        } else {
            ++filled_;
        }
        window_[writeIndex_] = input.value;
        sum_ += input.value;
        writeIndex_ = (writeIndex_ + 1U) % WindowSize;

        output = input;
        output.value = sum_ / static_cast<float>(filled_);
        if (filled_ < WindowSize) {
            output.quality |= QualityFlag::Estimated;
        }
        return okStatus();
    }

    [[nodiscard]] std::size_t filled() const noexcept { return filled_; }

private:
    Config config_;
    float window_[WindowSize]{};
    float sum_{0.0F};
    std::size_t writeIndex_{0};
    std::size_t filled_{0};
    bool initialized_{false};
};

}  // namespace mea
