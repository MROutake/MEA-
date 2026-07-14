#pragma once

/// @file NetworkMeasurementSink.h
/// @brief Protokoll-bewusster IMeasurementSink: verpackt Messwerte als
///        Measurement-Nachrichten und übergibt sie einem IMessagePublisher
///        (ProtocolBridge). Fügt sich unverändert in den Pipeline-/Orchestrator-
///        Fluss ein (ein Sink wie jeder andere, ADR 0005).

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include <mea/protocol/MessageEnvelope.h>

#include "IMessagePublisher.h"

namespace mea {
namespace network {

class NetworkMeasurementSink final : public IMeasurementSink {
public:
    /// @param publisher Nicht besitzend; muss den Sink überleben (ADR 0001).
    /// @param sinkId    Komponenten-ID dieses Sinks.
    /// @param targetId  Zielkomponente der Nachrichten (0 = Broadcast).
    NetworkMeasurementSink(IMessagePublisher& publisher, const ComponentId sinkId,
                           const ComponentId targetId = InvalidComponentId) noexcept
        : publisher_(publisher), sinkId_(sinkId), targetId_(targetId) {}

    [[nodiscard]] ComponentId id() const noexcept override { return sinkId_; }

    Status begin() noexcept override {
        initialized_ = false;
        if (sinkId_ == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidConfiguration, sinkId_);
        }
        sequence_ = 0;
        initialized_ = true;
        return okStatus();
    }

    Status update(const TimestampMs nowMs) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, sinkId_);
        }
        return publisher_.update(nowMs);
    }

    [[nodiscard]] std::size_t capacityAvailable() const noexcept override {
        return initialized_ ? 1U : 0U;
    }

    Status submit(const Measurement& measurement) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, sinkId_);
        }
        const protocol::MessageEnvelope envelope = protocol::makeMeasurementEnvelope(
            measurement.sourceId, targetId_, sequence_, measurement.sampledAtMs,
            measurement);
        Status status = publisher_.publish(envelope);
        if (status.ok()) {
            ++sequence_;
            return okStatus();
        }
        if (status.code == StatusCode::WouldBlock) {
            ++droppedCount_;  // Backpressure: Wert nicht übernommen
            return makeStatus(StatusCode::WouldBlock, sinkId_);
        }
        if (status.origin == InvalidComponentId) {
            status.origin = sinkId_;
        }
        return status;
    }

    /// Diagnose: wegen vollem TX-Puffer verworfene Messwerte.
    [[nodiscard]] std::uint32_t droppedCount() const noexcept { return droppedCount_; }

private:
    IMessagePublisher& publisher_;
    ComponentId sinkId_;
    ComponentId targetId_;
    SequenceNumber sequence_{0};
    std::uint32_t droppedCount_{0};
    bool initialized_{false};
};

}  // namespace network
}  // namespace mea
