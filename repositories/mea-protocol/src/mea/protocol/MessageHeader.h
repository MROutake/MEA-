#pragma once

/// @file MessageHeader.h
/// @brief Logischer Nachrichten-Header (in-memory) und Wire-Konstanten
///        (siehe PROTOCOL-SPEC §2). Der Header wird nie roh über die Leitung
///        gecastet, sondern feldweise little-endian (de)serialisiert.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "MessageKind.h"

namespace mea {
namespace protocol {

/// Magic am Frame-Anfang (ASCII "MA", little-endian 0x4D 0x41).
constexpr std::uint16_t kFrameMagic = 0x414DU;

/// Major-Version des Wire-Formats. Fremde Major-Versionen werden abgelehnt.
constexpr std::uint8_t kProtocolVersion = 1U;

/// Header-Länge auf der Leitung inklusive Magic.
constexpr std::size_t kHeaderSize = 18U;

/// Länge des CRC-Feldes am Frame-Ende.
constexpr std::size_t kCrcSize = 2U;

/// Maximale Payload-Länge in v0.1.
constexpr std::size_t kMaxPayloadSize = 255U;

/// Fixer Overhead je Frame (Header + CRC).
constexpr std::size_t kFrameOverhead = kHeaderSize + kCrcSize;

/// Maximale Frame-Länge (für Puffer-Dimensionierung im Composition Root).
constexpr std::size_t kMaxFrameSize = kFrameOverhead + kMaxPayloadSize;

/// Logischer Header. Trivial kopierbar; `magic` wird beim Encode gesetzt.
struct MessageHeader {
    std::uint16_t magic{kFrameMagic};
    std::uint8_t protocolVersion{kProtocolVersion};
    MessageKind kind{MessageKind::Unknown};
    ComponentId componentId{InvalidComponentId};
    ComponentId targetId{InvalidComponentId};
    SequenceNumber sequence{0};
    TimestampMs timestampMs{0};
    std::uint16_t payloadLength{0};
};

/// Broadcast-Ziel (kein bestimmter Empfänger).
constexpr ComponentId kBroadcastTarget = InvalidComponentId;

}  // namespace protocol
}  // namespace mea
