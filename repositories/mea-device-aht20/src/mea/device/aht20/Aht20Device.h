#pragma once

/// @file Aht20Device.h
/// @brief Nicht blockierende Messsteuerung für einen AHT20-Chip. Ein Chip
///        liefert zwei Messgrößen (Temperatur, Luftfeuchte); dieses Device
///        wird deshalb von mehreren Aht20Sensor-Kanälen geteilt (ADR 0001:
///        Eigentum beim Composition Root, das Device besitzt nur den Zustand
///        der Erfassung).
///
/// Ablauf (nicht blockierend, ADR 0004):
/// - poll() startet je Intervall eine Messung (triggerMeasurement) und fragt
///   danach das Ergebnis ab (readSample); Busy wird intern behandelt.
/// - poll() darf mehrfach je Tick aufgerufen werden (ein Aufruf pro Kanal);
///   Arbeit passiert nur, wenn sie fällig ist.
/// - Ein fertiges Sample bleibt als "letztes Sample" mit fortlaufender
///   sampleId() liegen; Kanäle erkennen neue Samples am Id-Wechsel.

#include <cstdint>

#include <MeaCore.h>

#include "IAht20Driver.h"

namespace mea {

class Aht20Device final : public IDevice {
public:
    struct Config {
        /// Abstand zwischen den Startzeitpunkten zweier Messungen (> 0).
        TimestampMs sampleIntervalMs{2000};
        /// Maximale Wartezeit auf ein Messergebnis (> 0); der AHT20 braucht
        /// typischerweise ca. 80 ms.
        TimestampMs measurementTimeoutMs{200};
    };

    /// @param driver Nicht besitzende Referenz; muss das Device überleben (ADR 0001).
    Aht20Device(IAht20Driver& driver, const Config& config) noexcept;

    /// Reinitialisierend: erneuter Aufruf verwirft eine laufende Messung;
    /// sampleId() und Diagnosezähler laufen weiter (ADR 0004).
    Status begin() noexcept override;

    /// Nicht blockierender Antrieb der Erfassung. Fehler einer Messung werden
    /// genau einmal gemeldet; der nächste Versuch startet nach dem Intervall.
    Status poll(TimestampMs nowMs) noexcept;

    /// IDevice-Antrieb (z. B. durch eine Runtime); identisch zu poll().
    Status update(const TimestampMs nowMs) noexcept override { return poll(nowMs); }

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    /// True, sobald mindestens ein Sample vorliegt.
    [[nodiscard]] bool hasSample() const noexcept { return sampleId_ != 0; }
    /// Monoton steigend je fertigem Sample; 0 bedeutet "noch kein Sample".
    [[nodiscard]] std::uint32_t sampleId() const noexcept { return sampleId_; }
    [[nodiscard]] const Aht20Sample& latestSample() const noexcept { return sample_; }
    /// Abschlusszeitpunkt des letzten Samples (ADR 0003).
    [[nodiscard]] TimestampMs sampledAtMs() const noexcept { return sampledAtMs_; }
    /// Diagnosezähler: abgebrochene Messungen (Triggerfehler, CRC, Timeout).
    [[nodiscard]] std::uint32_t failedAcquisitions() const noexcept {
        return failedAcquisitions_;
    }

private:
    IAht20Driver& driver_;
    Config config_;

    Aht20Sample sample_{};
    std::uint32_t sampleId_{0};
    TimestampMs sampledAtMs_{0};
    TimestampMs cycleStartMs_{0};
    std::uint32_t failedAcquisitions_{0};
    bool measuring_{false};
    bool firstCycle_{true};
    bool initialized_{false};
};

}  // namespace mea
