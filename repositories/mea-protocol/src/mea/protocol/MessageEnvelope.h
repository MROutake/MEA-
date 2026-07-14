#pragma once

/// @file MessageEnvelope.h
/// @brief Header + typisierter Payload einer Nachricht. Der `kind` im Header
///        diskriminiert die Union. Trivial kopierbar; keine Allokation.

#include <cstdint>
#include <type_traits>

#include <MeaCore.h>

#include "MessageHeader.h"
#include "MessageKind.h"
#include "Payloads.h"

namespace mea {
namespace protocol {

/// Ein vollständiges Protokoll-Nachrichtenobjekt (in-memory).
/// Zugriff auf das jeweilige Union-Feld nur passend zu `header.kind`.
struct MessageEnvelope {
    MessageHeader header{};

    union Payload {
        MeasurementPayload measurement;
        OutputStatePayload outputState;
        StateTransitionPayload stateTransition;
        ErrorEventPayload errorEvent;
        HeartbeatPayload heartbeat;
        CommandPayload command;

        // Union mit Membern, die Default-Member-Initializer besitzen, benötigt
        // einen benutzerdefinierten Default-Konstruktor (bleibt trivial kopierbar).
        Payload() noexcept : measurement{} {}
    } payload{};
};

static_assert(std::is_trivially_copyable<MessageEnvelope>::value,
              "MessageEnvelope muss trivial kopierbar bleiben");

/// Baut einen Header mit korrekt gesetzter Payload-Länge für @p kind.
[[nodiscard]] inline MessageHeader makeHeader(const MessageKind kind,
                                              const ComponentId componentId,
                                              const ComponentId targetId,
                                              const SequenceNumber sequence,
                                              const TimestampMs timestampMs) noexcept {
    MessageHeader header{};
    header.magic = kFrameMagic;
    header.protocolVersion = kProtocolVersion;
    header.kind = kind;
    header.componentId = componentId;
    header.targetId = targetId;
    header.sequence = sequence;
    header.timestampMs = timestampMs;
    header.payloadLength = static_cast<std::uint16_t>(payloadLengthFor(kind));
    return header;
}

/// @name Builder je Nachrichtentyp (Composition-Root-freundlich).
/// @{
[[nodiscard]] inline MessageEnvelope makeMeasurementEnvelope(
    const ComponentId componentId, const ComponentId targetId,
    const SequenceNumber sequence, const TimestampMs timestampMs,
    const MeasurementPayload& measurement) noexcept {
    MessageEnvelope envelope{};
    envelope.header = makeHeader(MessageKind::Measurement, componentId, targetId,
                                 sequence, timestampMs);
    envelope.payload.measurement = measurement;
    return envelope;
}

[[nodiscard]] inline MessageEnvelope makeOutputStateEnvelope(
    const ComponentId componentId, const ComponentId targetId,
    const SequenceNumber sequence, const TimestampMs timestampMs,
    const OutputStatePayload& outputState) noexcept {
    MessageEnvelope envelope{};
    envelope.header = makeHeader(MessageKind::OutputState, componentId, targetId,
                                 sequence, timestampMs);
    envelope.payload.outputState = outputState;
    return envelope;
}

[[nodiscard]] inline MessageEnvelope makeStateTransitionEnvelope(
    const ComponentId componentId, const ComponentId targetId,
    const SequenceNumber sequence, const TimestampMs timestampMs,
    const StateTransitionPayload& transition) noexcept {
    MessageEnvelope envelope{};
    envelope.header = makeHeader(MessageKind::StateTransition, componentId, targetId,
                                 sequence, timestampMs);
    envelope.payload.stateTransition = transition;
    return envelope;
}

[[nodiscard]] inline MessageEnvelope makeErrorEventEnvelope(
    const ComponentId componentId, const ComponentId targetId,
    const SequenceNumber sequence, const TimestampMs timestampMs,
    const ErrorEventPayload& error) noexcept {
    MessageEnvelope envelope{};
    envelope.header =
        makeHeader(MessageKind::ErrorEvent, componentId, targetId, sequence, timestampMs);
    envelope.payload.errorEvent = error;
    return envelope;
}

[[nodiscard]] inline MessageEnvelope makeHeartbeatEnvelope(
    const ComponentId componentId, const ComponentId targetId,
    const SequenceNumber sequence, const TimestampMs timestampMs,
    const HeartbeatPayload& heartbeat) noexcept {
    MessageEnvelope envelope{};
    envelope.header =
        makeHeader(MessageKind::Heartbeat, componentId, targetId, sequence, timestampMs);
    envelope.payload.heartbeat = heartbeat;
    return envelope;
}

[[nodiscard]] inline MessageEnvelope makeCommandEnvelope(
    const ComponentId componentId, const ComponentId targetId,
    const SequenceNumber sequence, const TimestampMs timestampMs,
    const CommandPayload& command) noexcept {
    MessageEnvelope envelope{};
    envelope.header =
        makeHeader(MessageKind::Command, componentId, targetId, sequence, timestampMs);
    envelope.payload.command = command;
    return envelope;
}
/// @}

}  // namespace protocol
}  // namespace mea
