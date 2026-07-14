#pragma once

/// @file ArduinoAnalogReader.h
/// @brief IAnalogReader-Implementierung auf Arduino-Basis (analogRead).
///        Einzige Arduino-abhängige Klasse dieser Library; nur unter ARDUINO
///        übersetzt.

#ifdef ARDUINO

#include <cstdint>

#include <MeaCore.h>

#include "IAnalogReader.h"

namespace mea {

class ArduinoAnalogReader final : public IAnalogReader {
public:
    /// @param maximumRawValue Größter Rohwert der Plattform (ESP32: 4095).
    ///        Wird bewusst nicht fest codiert (BoardConfig des Composition Roots).
    explicit ArduinoAnalogReader(std::uint32_t maximumRawValue) noexcept;

    Status beginPin(std::uint8_t pin) noexcept override;
    Status readRaw(std::uint8_t pin, std::uint32_t& output) noexcept override;
    [[nodiscard]] std::uint32_t maximumRawValue() const noexcept override;

private:
    std::uint32_t maximumRawValue_;
};

}  // namespace mea

#endif  // ARDUINO
