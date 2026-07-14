#pragma once

/// @file AnalogInputSensor.h
/// @brief Nicht blockierende analoge Messquelle mit Oversampling über eine
///        IAnalogReader-HAL. Keine Umrechnung in physikalische Einheiten –
///        das ist Aufgabe von mea-processing.
///
/// Ablauf (nicht blockierend, ADR 0004/0005):
/// - Pro update() werden höchstens maxSamplesPerUpdate Samples genommen;
///   die Summe wird über mehrere Updates akkumuliert.
/// - Ein fertiger Messwert (samplesPerMeasurement Samples) wird in einen
///   begrenzten Ringpuffer gelegt; sampledAtMs ist der Abschlusszeitpunkt.
/// - Drop-Policy bei vollem Puffer: Der NEUE Messwert wird verworfen und
///   droppedMeasurements() erhöht; die Sequenznummer zählt trotzdem weiter,
///   sodass Verluste an Sequenzlücken erkennbar sind.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IAnalogReader.h"

namespace mea {

class AnalogInputSensor final : public IMeasurementSource {
public:
    /// Kapazität des internen Messwertpuffers (RAM: 4 * sizeof(Measurement)).
    static constexpr std::size_t kQueueCapacity = 4;

    struct Config {
        ComponentId sourceId{InvalidComponentId};
        std::uint8_t pin{0};
        /// Abstand zwischen den Startzeitpunkten zweier Messungen (> 0).
        TimestampMs sampleIntervalMs{1000};
        /// Anzahl Samples je Messwert (> 0); Mittelwertbildung als Rohwert.
        std::uint16_t samplesPerMeasurement{1};
        /// Obergrenze Samples je update()-Aufruf (> 0, klein halten).
        std::uint8_t maxSamplesPerUpdate{4};
        MeasurementKind outputKind{MeasurementKind::RawAnalog};
        Unit outputUnit{Unit::RawCount};
    };

    /// @param reader Nicht besitzende Referenz; muss den Sensor überleben (ADR 0001).
    AnalogInputSensor(IAnalogReader& reader, const Config& config) noexcept;

    [[nodiscard]] ComponentId id() const noexcept override;

    /// Reinitialisierend: erneuter Aufruf verwirft laufende Messungen und den
    /// Pufferinhalt; Sequenznummer und Diagnosezähler laufen weiter (ADR 0004).
    Status begin() noexcept override;

    Status update(TimestampMs nowMs) noexcept override;
    [[nodiscard]] std::size_t available() const noexcept override;
    Status read(Measurement& output) noexcept override;

    /// Diagnosezähler: wegen vollem Puffer verworfene Messwerte.
    [[nodiscard]] std::uint32_t droppedMeasurements() const noexcept;
    /// Diagnosezähler: abgebrochene Messungen (HAL-Fehler, Summenüberlauf).
    [[nodiscard]] std::uint32_t failedAcquisitions() const noexcept;

private:
    Status takeSamples(TimestampMs nowMs) noexcept;
    void finishMeasurement(TimestampMs nowMs) noexcept;

    IAnalogReader& reader_;
    Config config_;
    RingBuffer<Measurement, kQueueCapacity> queue_{};

    std::uint32_t sampleSum_{0};
    std::uint16_t samplesTaken_{0};
    TimestampMs cycleStartMs_{0};
    SequenceNumber sequence_{0};
    std::uint32_t droppedMeasurements_{0};
    std::uint32_t failedAcquisitions_{0};
    bool acquiring_{false};
    bool firstCycle_{true};
    bool initialized_{false};
};

}  // namespace mea
