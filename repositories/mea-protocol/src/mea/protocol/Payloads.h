#pragma once

/// @file Payloads.h
/// @brief Payload-Strukturen der Nachrichtentypen (in-memory, siehe
///        PROTOCOL-SPEC §4). Alle trivial kopierbar; Serialisierung erfolgt
///        feldweise little-endian im BinaryMessageCodec.

#include <cstdint>
#include <type_traits>

#include <MeaCore.h>

namespace mea {
namespace protocol {

/// Maximale Anzahl Zustands-Bytes einer OutputState-Nachricht (64 Kanäle).
constexpr std::size_t kMaxOutputStateBytes = 8U;

/// Messwert-Payload = Abbild von mea::Measurement.
using MeasurementPayload = Measurement;

/// Ausgangszustand eines IC-/Treibers (z. B. 74HC595-Sink).
struct OutputStatePayload {
    ComponentId componentId{InvalidComponentId};
    std::uint8_t channelCount{0};
    std::uint8_t byteCount{0};
    std::uint8_t state[kMaxOutputStateBytes]{};
    TimestampMs appliedAtMs{0};
};

/// Pipeline-/Orchestrator-Zustandsübergang.
struct StateTransitionPayload {
    ComponentId componentId{InvalidComponentId};
    std::uint8_t fromState{0};
    std::uint8_t toState{0};
    std::uint16_t reason{0};
    std::uint16_t flags{0};
    TimestampMs atMs{0};
};

/// Fehlerereignis = Abbild von mea::Status mit Zeitpunkt und Schweregrad.
struct ErrorEventPayload {
    ComponentId componentId{InvalidComponentId};
    std::uint8_t code{0};      ///< StatusCode
    std::uint8_t severity{0};  ///< 0 = info, 1 = warn, 2 = error
    std::uint16_t detail{0};
    std::uint16_t flags{0};
    TimestampMs atMs{0};
};

/// Lebenszeichen eines Node.
struct HeartbeatPayload {
    ComponentId componentId{InvalidComponentId};
    TimestampMs uptimeMs{0};
    SequenceNumber sequence{0};
    std::uint16_t flags{0};
    std::uint8_t state{0};
    std::uint8_t reserved{0};
};

/// Eingehendes Kommando = Abbild von mea::Command (optional).
struct CommandPayload {
    ComponentId sourceId{InvalidComponentId};
    ComponentId targetId{InvalidComponentId};
    std::uint16_t type{0};  ///< CommandType
    std::uint32_t argument{0};
    TimestampMs atMs{0};
};

static_assert(std::is_trivially_copyable<OutputStatePayload>::value,
              "OutputStatePayload muss trivial kopierbar sein");
static_assert(std::is_trivially_copyable<StateTransitionPayload>::value,
              "StateTransitionPayload muss trivial kopierbar sein");
static_assert(std::is_trivially_copyable<ErrorEventPayload>::value,
              "ErrorEventPayload muss trivial kopierbar sein");
static_assert(std::is_trivially_copyable<HeartbeatPayload>::value,
              "HeartbeatPayload muss trivial kopierbar sein");
static_assert(std::is_trivially_copyable<CommandPayload>::value,
              "CommandPayload muss trivial kopierbar sein");

}  // namespace protocol
}  // namespace mea
