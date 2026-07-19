#pragma once

/// @file ArduinoAht20Driver.h
/// @brief IAht20Driver-Implementierung über Arduino-Wire (I2C). Einzige
///        Arduino-abhängige Klasse dieser Library; nur unter ARDUINO
///        übersetzt. Ohne externe Sensor-Libraries: das AHT20-Protokoll
///        (Trigger 0xAC, Statusbit, 20-Bit-Rohwerte, CRC-8) ist direkt
///        implementiert, damit readSample() nicht blockiert.
///
/// Voraussetzung: Wire.begin(SDA, SCL) ist Aufgabe des Composition Roots und
/// muss vor begin() erfolgt sein.

#ifdef ARDUINO

#include <Wire.h>

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IAht20Driver.h"

namespace mea {

class ArduinoAht20Driver final : public IAht20Driver {
public:
    static constexpr std::uint8_t kDefaultAddress = 0x38;

    /// @param wire Nicht besitzende Referenz; muss den Treiber überleben (ADR 0001).
    explicit ArduinoAht20Driver(TwoWire& wire,
                                std::uint8_t address = kDefaultAddress) noexcept;

    /// Prüft die Kalibrierung und initialisiert den Chip bei Bedarf. Wartet
    /// dabei kurz (einmalige Initialisierung, kein update()-Pfad, ADR 0004).
    Status begin() noexcept override;

    Status triggerMeasurement() noexcept override;
    Status readSample(Aht20Sample& output) noexcept override;

private:
    Status readStatusByte(std::uint8_t& statusByte) noexcept;
    Status writeCommand(std::uint8_t command, std::uint8_t parameter1,
                        std::uint8_t parameter2) noexcept;
    [[nodiscard]] static std::uint8_t crc8(const std::uint8_t* data,
                                           std::size_t length) noexcept;

    TwoWire& wire_;
    std::uint8_t address_;
};

}  // namespace mea

#endif  // ARDUINO
