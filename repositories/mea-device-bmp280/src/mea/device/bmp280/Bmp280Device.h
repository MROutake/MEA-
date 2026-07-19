#pragma once

/// @file Bmp280Device.h
/// @brief Nicht blockierende Messsteuerung für einen BMP280-Chip. Ein Chip
///        liefert zwei Messgrößen (Temperatur, Luftdruck); dieses Device wird
///        deshalb von mehreren Bmp280Sensor-Kanälen geteilt (ADR 0001).
///
/// Ablauf (nicht blockierend, ADR 0004):
/// - Der BMP280 misst im Normal Mode kontinuierlich; poll() liest je Intervall
///   das letzte fertige Sample über den Treiber.
/// - Busy (erste Messung nach begin() läuft noch) wird intern behandelt und
///   beim nächsten poll() erneut versucht.
/// - poll() darf mehrfach je Tick aufgerufen werden (ein Aufruf pro Kanal).
/// - Ein fertiges Sample bleibt als "letztes Sample" mit fortlaufender
///   sampleId() liegen; Kanäle erkennen neue Samples am Id-Wechsel.

#include <cstdint>

#include <MeaCore.h>

#include "IBmp280Driver.h"

namespace mea {

class Bmp280Device final : public IDevice {
public:
    struct Config {
        /// Abstand zwischen zwei Sample-Lesungen (> 0).
        TimestampMs sampleIntervalMs{2000};
    };

    /// @param driver Nicht besitzende Referenz; muss das Device überleben (ADR 0001).
    Bmp280Device(IBmp280Driver& driver, const Config& config) noexcept;

    /// Reinitialisierend: sampleId() und Diagnosezähler laufen weiter (ADR 0004).
    Status begin() noexcept override;

    /// Nicht blockierender Antrieb der Erfassung. Fehler einer Lesung werden
    /// genau einmal gemeldet; der nächste Versuch startet nach dem Intervall.
    Status poll(TimestampMs nowMs) noexcept;

    /// IDevice-Antrieb (z. B. durch eine Runtime); identisch zu poll().
    Status update(const TimestampMs nowMs) noexcept override { return poll(nowMs); }

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    /// True, sobald mindestens ein Sample vorliegt.
    [[nodiscard]] bool hasSample() const noexcept { return sampleId_ != 0; }
    /// Monoton steigend je gelesenem Sample; 0 bedeutet "noch kein Sample".
    [[nodiscard]] std::uint32_t sampleId() const noexcept { return sampleId_; }
    [[nodiscard]] const Bmp280Sample& latestSample() const noexcept { return sample_; }
    /// Abschlusszeitpunkt des letzten Samples (ADR 0003).
    [[nodiscard]] TimestampMs sampledAtMs() const noexcept { return sampledAtMs_; }
    /// Diagnosezähler: fehlgeschlagene Sample-Lesungen.
    [[nodiscard]] std::uint32_t failedAcquisitions() const noexcept {
        return failedAcquisitions_;
    }

private:
    IBmp280Driver& driver_;
    Config config_;

    Bmp280Sample sample_{};
    std::uint32_t sampleId_{0};
    TimestampMs sampledAtMs_{0};
    TimestampMs cycleStartMs_{0};
    std::uint32_t failedAcquisitions_{0};
    bool firstCycle_{true};
    bool initialized_{false};
};

}  // namespace mea
