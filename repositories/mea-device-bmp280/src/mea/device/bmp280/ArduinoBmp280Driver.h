#pragma once

/// @file ArduinoBmp280Driver.h
/// @brief IBmp280Driver-Implementierung über Arduino-Wire (I2C). Einzige
///        Arduino-abhängige Klasse dieser Library; nur unter ARDUINO
///        übersetzt. Ohne externe Sensor-Libraries: Chip-ID-Prüfung,
///        Kalibrierkoeffizienten und Kompensationsrechnung (Datenblatt Bosch
///        BMP280, Kap. 3.11.3/8.1) sind direkt implementiert.
///
/// Der Chip wird in den Normal Mode versetzt (kontinuierliche Messung,
/// Oversampling T x2 / P x16, IIR-Filter 4, Standby 250 ms); readSample()
/// liest nur Register und blockiert nicht. Ein BME280 (Chip-ID 0x60) wird
/// ebenfalls akzeptiert; dessen Feuchtekanal bleibt ungenutzt.
///
/// Voraussetzung: Wire.begin(SDA, SCL) ist Aufgabe des Composition Roots und
/// muss vor begin() erfolgt sein.

#ifdef ARDUINO

#include <Wire.h>

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IBmp280Driver.h"

namespace mea {

class ArduinoBmp280Driver final : public IBmp280Driver {
public:
    /// Adresse 0x77 auf den AHT20+BMP280-Kombiboards; einzeln meist 0x76.
    static constexpr std::uint8_t kDefaultAddress = 0x77;

    /// @param wire Nicht besitzende Referenz; muss den Treiber überleben (ADR 0001).
    explicit ArduinoBmp280Driver(TwoWire& wire,
                                 std::uint8_t address = kDefaultAddress) noexcept;

    Status begin() noexcept override;
    Status readSample(Bmp280Sample& output) noexcept override;

private:
    Status readRegisters(std::uint8_t startRegister, std::uint8_t* buffer,
                         std::size_t length) noexcept;
    Status writeRegister(std::uint8_t registerAddress, std::uint8_t value) noexcept;
    Status readCalibration() noexcept;

    TwoWire& wire_;
    std::uint8_t address_;

    // Kalibrierkoeffizienten (Datenblatt Kap. 3.11.2)
    std::uint16_t digT1_{0};
    std::int16_t digT2_{0};
    std::int16_t digT3_{0};
    std::uint16_t digP1_{0};
    std::int16_t digP2_{0};
    std::int16_t digP3_{0};
    std::int16_t digP4_{0};
    std::int16_t digP5_{0};
    std::int16_t digP6_{0};
    std::int16_t digP7_{0};
    std::int16_t digP8_{0};
    std::int16_t digP9_{0};
};

}  // namespace mea

#endif  // ARDUINO
