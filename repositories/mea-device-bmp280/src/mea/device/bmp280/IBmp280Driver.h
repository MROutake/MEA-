#pragma once

/// @file IBmp280Driver.h
/// @brief Hardwareabstraktion für den BMP280 (Temperatur/Luftdruck, I2C).
///        Trennt Bmp280Device von konkreter Hardware (Arduino/Wire) und macht
///        die Messlogik nativ testbar.
///
/// Der Treiber liefert bereits kompensierte physikalische Größen; die
/// Kalibrierkoeffizienten und die Kompensationsrechnung sind Treibersache.

#include <MeaCore.h>

namespace mea {

/// Ein vollständiges BMP280-Sample.
struct Bmp280Sample {
    float temperatureCelsius{0.0F};
    float pressurePascal{0.0F};
};

class IBmp280Driver {
public:
    virtual ~IBmp280Driver() = default;

    /// Initialisiert den Chip (ID prüfen, Kalibrierung lesen, Dauermessmodus).
    /// Keine Hardwarearbeit im Konstruktor.
    virtual Status begin() noexcept = 0;

    /// Liest das letzte fertige Sample (nicht blockierend; der Chip misst im
    /// Normal Mode kontinuierlich). Busy, solange nach begin() noch keine
    /// erste Messung abgeschlossen ist.
    virtual Status readSample(Bmp280Sample& output) noexcept = 0;
};

}  // namespace mea
