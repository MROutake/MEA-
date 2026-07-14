#pragma once

/// @file FakeAnalogReader.h
/// @brief IAnalogReader-Fake für native Tests: konfigurierbare Wertefolge,
///        injizierbare Fehler, Zähler. Keine Arduino-Abhängigkeit.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "../IAnalogReader.h"

namespace mea::testing {

class FakeAnalogReader final : public IAnalogReader {
public:
    static constexpr std::size_t kMaxValues = 8;

    Status beginPin(const std::uint8_t pin) noexcept override {
        ++beginPinCalls;
        lastPin = pin;
        return beginPinResult;
    }

    Status readRaw(const std::uint8_t pin, std::uint32_t& output) noexcept override {
        lastPin = pin;
        if (failAfterReads != 0 && readCalls >= failAfterReads) {
            ++readCalls;
            return readFailure;
        }
        ++readCalls;
        if (valueCount == 0) {
            output = 0;
            return okStatus();
        }
        output = values[valueIndex];
        valueIndex = (valueIndex + 1U) % valueCount;
        return okStatus();
    }

    [[nodiscard]] std::uint32_t maximumRawValue() const noexcept override {
        return maximumRaw;
    }

    /// Liefert ab jetzt zyklisch die ersten @p count Werte aus @p newValues.
    void setValues(const std::uint32_t* newValues, const std::size_t count) noexcept {
        valueCount = count < kMaxValues ? count : kMaxValues;
        for (std::size_t index = 0; index < valueCount; ++index) {
            values[index] = newValues[index];
        }
        valueIndex = 0;
    }

    void setConstantValue(const std::uint32_t value) noexcept {
        values[0] = value;
        valueCount = 1;
        valueIndex = 0;
    }

    Status beginPinResult{okStatus()};
    /// 0 = nie fehlschlagen; sonst schlägt readRaw ab dem n-ten Aufruf fehl.
    std::uint32_t failAfterReads{0};
    Status readFailure{makeStatus(StatusCode::IoError, InvalidComponentId, 0)};
    std::uint32_t maximumRaw{4095};

    std::uint32_t beginPinCalls{0};
    std::uint32_t readCalls{0};
    std::uint8_t lastPin{0};

private:
    std::uint32_t values[kMaxValues]{};
    std::size_t valueCount{0};
    std::size_t valueIndex{0};
};

}  // namespace mea::testing
