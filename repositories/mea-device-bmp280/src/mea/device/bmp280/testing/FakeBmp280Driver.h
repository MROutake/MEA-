#pragma once

/// @file FakeBmp280Driver.h
/// @brief IBmp280Driver-Fake für native Tests: konfigurierbares Sample,
///        einstellbare Busy-Phase, injizierbare Fehler, Zähler. Keine
///        Arduino-Abhängigkeit.

#include <cstdint>

#include <MeaCore.h>

#include "../IBmp280Driver.h"

namespace mea::testing {

class FakeBmp280Driver final : public IBmp280Driver {
public:
    Status begin() noexcept override {
        ++beginCalls;
        return beginResult;
    }

    Status readSample(Bmp280Sample& output) noexcept override {
        ++readCalls;
        if (!readResult.ok()) {
            return readResult;
        }
        if (busyReadsRemaining > 0) {
            --busyReadsRemaining;
            return makeStatus(StatusCode::Busy, InvalidComponentId);
        }
        output = sample;
        return okStatus();
    }

    /// Konfiguration
    Bmp280Sample sample{};
    Status beginResult{okStatus()};
    Status readResult{okStatus()};
    /// Anzahl readSample()-Aufrufe, die (noch) Busy melden.
    std::uint32_t busyReadsRemaining{0};

    /// Zähler
    std::uint32_t beginCalls{0};
    std::uint32_t readCalls{0};
};

}  // namespace mea::testing
