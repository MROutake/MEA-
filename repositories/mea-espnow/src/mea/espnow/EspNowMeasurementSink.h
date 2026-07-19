#pragma once

/// @file EspNowMeasurementSink.h
/// @brief Measurement-Sink über ESP-NOW: kodiert je Messwert einen Frame
///        (IMeasurementEncoder aus mea-communication) und sendet ihn als
///        Data-Datagramm über eine IEspNowSession (ADR 0008).
///
/// Drop-Policy (bewusst anders als BufferedMeasurementSink, dokumentiert in
/// ADR 0008): Die Funkstrecke ist inhärent verlustbehaftet. Ist die Queue
/// voll (z. B. weil kein Server verbunden ist), wird der ÄLTESTE Messwert
/// verworfen und der neue übernommen — frische Daten schlagen alte, die
/// Pipelines geraten nicht in Backpressure, Verluste sind über
/// droppedMeasurements() und Sequenzlücken beim Empfänger sichtbar.
///
/// RAM je Instanz: QueueCapacity * sizeof(Measurement) + kEspNowMaxPayload.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>
#include <mea/communication/IMeasurementEncoder.h>

#include "EspNowTypes.h"

namespace mea {

/// @tparam QueueCapacity Kapazität der Messwert-Queue (> 0).
template <std::size_t QueueCapacity = 8>
class EspNowMeasurementSink final : public IMeasurementSink {
    static_assert(QueueCapacity > 0, "QueueCapacity muss > 0 sein");

public:
    /// Obergrenze gesendeter Frames je update()-Aufruf (begrenzte Arbeit).
    static constexpr std::size_t kMaxSendsPerUpdate = 2;

    /// @param session Nicht besitzend (z. B. EspNowClient); muss den Sink
    ///        überleben (ADR 0001).
    /// @param encoder Nicht besitzend; eine kodierte Zeile muss in
    ///        kEspNowMaxPayload Bytes passen.
    EspNowMeasurementSink(IEspNowSession& session, const IMeasurementEncoder& encoder,
                          const ComponentId sinkId) noexcept
        : session_(session), encoder_(encoder), sinkId_(sinkId) {}

    [[nodiscard]] ComponentId id() const noexcept override { return sinkId_; }

    /// Reinitialisierend: leert die Queue; Diagnosezähler laufen weiter.
    Status begin() noexcept override {
        initialized_ = false;
        if (sinkId_ == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidConfiguration, sinkId_);
        }
        queue_.clear();
        initialized_ = true;
        return okStatus();
    }

    Status update(TimestampMs) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, sinkId_);
        }
        if (!session_.connected()) {
            return okStatus();  // Messwerte warten in der Queue (Drop-Oldest)
        }

        for (std::size_t sent = 0; sent < kMaxSendsPerUpdate; ++sent) {
            const Measurement* const next = queue_.front();
            if (next == nullptr) {
                return okStatus();
            }

            std::uint8_t payload[kEspNowMaxPayload];
            std::size_t encodedSize = 0;
            Status status =
                encoder_.encode(*next, payload, kEspNowMaxPayload, encodedSize);
            if (!status.ok()) {
                // Messwert nicht kodierbar: verwerfen und melden.
                Measurement dropped{};
                (void)queue_.pop(dropped);
                ++encodeErrors_;
                status.origin = sinkId_;
                return status;
            }

            status = session_.sendData(payload, encodedSize);
            if (status.code == StatusCode::WouldBlock) {
                return okStatus();  // Verbindung gerade weg: später erneut
            }
            if (!status.ok()) {
                // Frame behalten und beim nächsten update() erneut versuchen.
                ++sendFailures_;
                if (status.origin == InvalidComponentId) {
                    status.origin = sinkId_;
                }
                return status;
            }
            Measurement sentMeasurement{};
            (void)queue_.pop(sentMeasurement);
            ++sentFrames_;
        }
        return okStatus();
    }

    [[nodiscard]] std::size_t capacityAvailable() const noexcept override {
        return QueueCapacity - queue_.size();
    }

    Status submit(const Measurement& measurement) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, sinkId_);
        }
        if (queue_.full()) {
            Measurement dropped{};
            (void)queue_.pop(dropped);  // Drop-Oldest (siehe Dateikopf)
            ++droppedMeasurements_;
        }
        (void)queue_.push(measurement);
        return okStatus();
    }

    /// Diagnose: wegen voller Queue verworfene (älteste) Messwerte.
    [[nodiscard]] std::uint32_t droppedMeasurements() const noexcept {
        return droppedMeasurements_;
    }
    /// Diagnose: nicht kodierbare (verworfene) Messwerte.
    [[nodiscard]] std::uint32_t encodeErrors() const noexcept { return encodeErrors_; }
    /// Diagnose: fehlgeschlagene Sendeversuche (Frame wird erneut versucht).
    [[nodiscard]] std::uint32_t sendFailures() const noexcept { return sendFailures_; }
    /// Diagnose: erfolgreich gesendete Data-Frames.
    [[nodiscard]] std::uint32_t sentFrames() const noexcept { return sentFrames_; }

private:
    IEspNowSession& session_;
    const IMeasurementEncoder& encoder_;
    ComponentId sinkId_;

    RingBuffer<Measurement, QueueCapacity> queue_{};

    std::uint32_t droppedMeasurements_{0};
    std::uint32_t encodeErrors_{0};
    std::uint32_t sendFailures_{0};
    std::uint32_t sentFrames_{0};
    bool initialized_{false};
};

}  // namespace mea
