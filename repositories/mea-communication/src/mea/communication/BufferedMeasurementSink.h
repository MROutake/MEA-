#pragma once

/// @file BufferedMeasurementSink.h
/// @brief Gepufferter Measurement-Sink (ADR 0006, Schicht 3).
///
/// - submit() kopiert den Messwert in eine feste Queue; volle Queue meldet
///   WouldBlock (Backpressure), der Wert wird nicht übernommen und
///   rejectedSubmits() erhöht – kein stiller Verlust.
/// - update() kodiert je Messwert einen Frame und schreibt ihn nicht
///   blockierend in den Transport; partielle Writes werden über mehrere
///   Updates fortgesetzt. Die Arbeit pro update() ist durch QueueCapacity
///   begrenzt.
/// - Encoderfehler verwerfen den betroffenen Messwert (encodeErrors()),
///   Transportfehler behalten den Frame für den nächsten Versuch
///   (transportErrors()).
/// - Lebenszyklus des Transports (begin/update) liegt beim Composition Root.
///
/// RAM je Instanz: QueueCapacity * sizeof(Measurement) + FrameSize Bytes.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IByteTransport.h"
#include "IMeasurementEncoder.h"

namespace mea {

/// @tparam QueueCapacity Kapazität der Messwert-Queue (> 0).
/// @tparam FrameSize     Maximale Framelänge in Bytes (> 0), muss für eine
///                       kodierte Zeile ausreichen.
template <std::size_t QueueCapacity, std::size_t FrameSize>
class BufferedMeasurementSink final : public IMeasurementSink {
    static_assert(QueueCapacity > 0, "QueueCapacity muss > 0 sein");
    static_assert(FrameSize > 0, "FrameSize muss > 0 sein");

public:
    /// @param transport Nicht besitzend; muss den Sink überleben (ADR 0001).
    /// @param encoder   Nicht besitzend; muss den Sink überleben.
    BufferedMeasurementSink(IByteTransport& transport, const IMeasurementEncoder& encoder,
                            const ComponentId sinkId) noexcept
        : transport_(transport), encoder_(encoder), sinkId_(sinkId) {}

    [[nodiscard]] ComponentId id() const noexcept override { return sinkId_; }

    /// Reinitialisierend: erneuter Aufruf leert Queue und Framepuffer;
    /// Diagnosezähler laufen weiter (ADR 0004).
    Status begin() noexcept override {
        initialized_ = false;
        if (sinkId_ == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidConfiguration, sinkId_);
        }
        queue_.clear();
        frameLength_ = 0;
        frameOffset_ = 0;
        initialized_ = true;
        return okStatus();
    }

    Status update(TimestampMs) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, sinkId_);
        }

        // Begrenzte Arbeit: höchstens QueueCapacity Frames plus den
        // angefangenen Frame fortsetzen.
        for (std::size_t iteration = 0; iteration <= QueueCapacity; ++iteration) {
            if (frameLength_ == 0) {
                Measurement measurement{};
                if (!queue_.pop(measurement)) {
                    return okStatus();  // nichts mehr zu senden
                }
                std::size_t encoded = 0;
                Status status = encoder_.encode(measurement, frame_, FrameSize, encoded);
                if (!status.ok()) {
                    // Messwert nicht kodierbar: verwerfen und melden.
                    ++encodeErrors_;
                    status.origin = sinkId_;
                    return status;
                }
                frameLength_ = encoded;
                frameOffset_ = 0;
            }

            std::size_t written = 0;
            Status status = transport_.write(frame_ + frameOffset_,
                                             frameLength_ - frameOffset_, written);
            if (!status.ok()) {
                // Frame behalten und beim nächsten update() erneut versuchen.
                ++transportErrors_;
                if (status.origin == InvalidComponentId) {
                    status.origin = sinkId_;
                }
                return status;
            }
            frameOffset_ += written;
            if (frameOffset_ < frameLength_) {
                return okStatus();  // Transport voll: später fortsetzen
            }
            frameLength_ = 0;
            frameOffset_ = 0;
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
        if (!queue_.push(measurement)) {
            ++rejectedSubmits_;
            return makeStatus(StatusCode::WouldBlock, sinkId_);
        }
        return okStatus();
    }

    /// Diagnose: wegen voller Queue abgelehnte submit()-Aufrufe.
    [[nodiscard]] std::uint32_t rejectedSubmits() const noexcept {
        return rejectedSubmits_;
    }
    /// Diagnose: nicht kodierbare (verworfene) Messwerte.
    [[nodiscard]] std::uint32_t encodeErrors() const noexcept { return encodeErrors_; }
    /// Diagnose: fehlgeschlagene Transport-Writes (Frame wird erneut versucht).
    [[nodiscard]] std::uint32_t transportErrors() const noexcept {
        return transportErrors_;
    }
    /// Diagnose: vollständig gesendete Frames.
    [[nodiscard]] std::uint32_t sentFrames() const noexcept { return sentFrames_; }

private:
    IByteTransport& transport_;
    const IMeasurementEncoder& encoder_;
    ComponentId sinkId_;

    RingBuffer<Measurement, QueueCapacity> queue_{};
    std::uint8_t frame_[FrameSize]{};
    std::size_t frameLength_{0};
    std::size_t frameOffset_{0};

    std::uint32_t rejectedSubmits_{0};
    std::uint32_t encodeErrors_{0};
    std::uint32_t transportErrors_{0};
    std::uint32_t sentFrames_{0};
    bool initialized_{false};
};

}  // namespace mea
