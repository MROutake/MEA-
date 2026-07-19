#pragma once

/// @file FakeAht20Driver.h
/// @brief IAht20Driver-Fake für native Tests: konfigurierbares Sample,
///        einstellbare Busy-Dauer, injizierbare Fehler, Zähler. Keine
///        Arduino-Abhängigkeit.

#include <cstdint>

#include <MeaCore.h>

#include "../IAht20Driver.h"

namespace mea::testing {

class FakeAht20Driver final : public IAht20Driver {
public:
    Status begin() noexcept override {
        ++beginCalls;
        return beginResult;
    }

    Status triggerMeasurement() noexcept override {
        ++triggerCalls;
        if (!triggerResult.ok()) {
            return triggerResult;
        }
        measuring_ = true;
        busyPollsRemaining_ = busyPollsPerMeasurement;
        return okStatus();
    }

    Status readSample(Aht20Sample& output) noexcept override {
        ++readCalls;
        if (!readResult.ok()) {
            measuring_ = false;
            return readResult;
        }
        if (!measuring_ || busyPollsRemaining_ > 0) {
            if (busyPollsRemaining_ > 0) {
                --busyPollsRemaining_;
            }
            return makeStatus(StatusCode::Busy, InvalidComponentId);
        }
        measuring_ = false;
        output = sample;
        return okStatus();
    }

    /// Konfiguration
    Aht20Sample sample{};
    Status beginResult{okStatus()};
    Status triggerResult{okStatus()};
    Status readResult{okStatus()};
    /// Anzahl readSample()-Aufrufe, die nach einem Trigger Busy melden.
    std::uint32_t busyPollsPerMeasurement{0};

    /// Zähler
    std::uint32_t beginCalls{0};
    std::uint32_t triggerCalls{0};
    std::uint32_t readCalls{0};

private:
    std::uint32_t busyPollsRemaining_{0};
    bool measuring_{false};
};

}  // namespace mea::testing
