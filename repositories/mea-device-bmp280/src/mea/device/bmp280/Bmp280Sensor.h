#pragma once

/// @file Bmp280Sensor.h
/// @brief Messquelle für genau eine BMP280-Messgröße (Kanal). Zwei Kanäle
///        (Temperatur, Luftdruck) teilen sich ein Bmp280Device; jeder Kanal
///        hat eine eigene ComponentId, Queue und Sequenznummer und lässt sich
///        damit als eigenständige Quelle in eine Pipeline verdrahten.
///
/// Reihenfolge im Composition Root (ADR 0004): erst Bmp280Device::begin(),
/// dann die Kanäle (z. B. über SensorManager::beginAll()).
///
/// Drop-Policy bei vollem Puffer: Der NEUE Messwert wird verworfen und
/// droppedMeasurements() erhöht; die Sequenznummer zählt trotzdem weiter,
/// sodass Verluste an Sequenzlücken erkennbar sind.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "Bmp280Device.h"

namespace mea {

class Bmp280Sensor final : public IMeasurementSource {
public:
    /// Kapazität des internen Messwertpuffers (RAM: 4 * sizeof(Measurement)).
    static constexpr std::size_t kQueueCapacity = 4;

    enum class Channel : std::uint8_t { Temperature, Pressure };

    struct Config {
        ComponentId sourceId{InvalidComponentId};
        Channel channel{Channel::Temperature};
    };

    /// @param device Nicht besitzende Referenz; muss den Sensor überleben (ADR 0001).
    Bmp280Sensor(Bmp280Device& device, const Config& config) noexcept;

    [[nodiscard]] ComponentId id() const noexcept override;

    /// Reinitialisierend: erneuter Aufruf verwirft den Pufferinhalt und noch
    /// nicht übernommene Samples; die Sequenznummer läuft weiter (ADR 0004).
    /// Meldet NotInitialized, wenn das Device noch nicht initialisiert ist.
    Status begin() noexcept override;

    Status update(TimestampMs nowMs) noexcept override;
    [[nodiscard]] std::size_t available() const noexcept override;
    Status read(Measurement& output) noexcept override;

    /// Diagnosezähler: wegen vollem Puffer verworfene Messwerte.
    [[nodiscard]] std::uint32_t droppedMeasurements() const noexcept;

private:
    void takeNewSample() noexcept;

    Bmp280Device& device_;
    Config config_;
    RingBuffer<Measurement, kQueueCapacity> queue_{};

    std::uint32_t lastSampleId_{0};
    SequenceNumber sequence_{0};
    std::uint32_t droppedMeasurements_{0};
    bool initialized_{false};
};

}  // namespace mea
