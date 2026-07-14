#pragma once

/// @file IAnalogReader.h
/// @brief Hardwareabstraktion für analoge Eingänge. Trennt AnalogInputSensor
///        von konkreter Hardware (Arduino) und macht ihn nativ testbar.

#include <cstdint>

#include <MeaCore.h>

namespace mea {

class IAnalogReader {
public:
    virtual ~IAnalogReader() = default;

    /// Bereitet den Pin vor (z. B. pinMode). Keine Hardwarearbeit im Konstruktor.
    virtual Status beginPin(std::uint8_t pin) noexcept = 0;

    /// Liest einen Rohwert; blockiert nicht länger als eine einzelne Wandlung.
    virtual Status readRaw(std::uint8_t pin, std::uint32_t& output) noexcept = 0;

    /// Größter möglicher Rohwert (z. B. 4095 bei 12 Bit).
    [[nodiscard]] virtual std::uint32_t maximumRawValue() const noexcept = 0;
};

}  // namespace mea
