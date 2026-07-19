#pragma once

/// @file IAht20Driver.h
/// @brief Hardwareabstraktion für den AHT20 (Temperatur/Luftfeuchte, I2C).
///        Trennt Aht20Device von konkreter Hardware (Arduino/Wire) und macht
///        die Messlogik nativ testbar.
///
/// Der Treiber liefert bereits umgerechnete physikalische Größen; die
/// Rohdaten-Dekodierung (20-Bit-Werte, CRC) ist Treibersache.

#include <MeaCore.h>

namespace mea {

/// Ein vollständiges AHT20-Sample.
struct Aht20Sample {
    float temperatureCelsius{0.0F};
    float humidityPercent{0.0F};
};

class IAht20Driver {
public:
    virtual ~IAht20Driver() = default;

    /// Initialisiert und kalibriert den Chip. Keine Hardwarearbeit im
    /// Konstruktor; darf während der Initialisierung kurz warten.
    virtual Status begin() noexcept = 0;

    /// Startet eine Messung und kehrt sofort zurück (nicht blockierend).
    virtual Status triggerMeasurement() noexcept = 0;

    /// Holt das Messergebnis ab. Busy, solange die Messung noch läuft;
    /// ChecksumError, wenn die CRC-Prüfung fehlschlägt (Sample verworfen).
    virtual Status readSample(Aht20Sample& output) noexcept = 0;
};

}  // namespace mea
