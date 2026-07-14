#pragma once

/// @file PassThroughProcessor.h
/// @brief Reicht Messwerte unverändert weiter (nützlich für Tests und als
///        neutrales Kettenglied). Akzeptiert jede Art und Einheit.

#include <MeaCore.h>

namespace mea {

class PassThroughProcessor final : public IMeasurementProcessor {
public:
    explicit PassThroughProcessor(const ComponentId processorId) noexcept
        : processorId_(processorId) {}

    [[nodiscard]] ComponentId id() const noexcept override { return processorId_; }

    /// Reinitialisierend: erneuter Aufruf ist erlaubt und gibt Ok zurück (ADR 0004).
    Status begin() noexcept override {
        if (processorId_ == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidConfiguration, processorId_);
        }
        initialized_ = true;
        return okStatus();
    }

    [[nodiscard]] bool accepts(MeasurementKind, Unit) const noexcept override {
        return true;
    }

    Status process(const Measurement& input, Measurement& output) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, processorId_);
        }
        if (!hasFiniteValue(input)) {
            return makeStatus(StatusCode::InvalidArgument, processorId_);
        }
        output = input;
        return okStatus();
    }

private:
    ComponentId processorId_;
    bool initialized_{false};
};

}  // namespace mea
