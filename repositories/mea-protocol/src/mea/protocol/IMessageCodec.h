#pragma once

/// @file IMessageCodec.h
/// @brief Encoder/Decoder-Interfaces (siehe PROTOCOL-SPEC §5). Serialisierung
///        ausschließlich in/aus Aufruferpuffern, keine Allokation (ADR 0006).

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "MessageEnvelope.h"

namespace mea {
namespace protocol {

/// Serialisiert eine Nachricht in einen Aufruferpuffer.
class IMessageEncoder {
public:
    virtual ~IMessageEncoder() = default;

    /// Schreibt einen vollständigen Frame nach @p output (höchstens @p capacity
    /// Byte). @p written meldet die geschriebene Länge.
    /// Fehler: InvalidArgument (Nullzeiger/ungültiger Kind), CapacityExceeded
    /// (@p detail = benötigte Länge).
    virtual Status encode(const MessageEnvelope& envelope, std::uint8_t* output,
                          std::size_t capacity,
                          std::size_t& written) const noexcept = 0;
};

/// Deserialisiert einen Frame aus einem Aufruferpuffer.
class IMessageDecoder {
public:
    virtual ~IMessageDecoder() = default;

    /// Liest genau einen Frame ab @p input (bis zu @p size Byte). @p consumed
    /// meldet die Byte-Anzahl des erkannten Frames (für das Nachrücken im
    /// Empfangspuffer, auch bei ChecksumError/ProtocolError des gefundenen
    /// Frames). Fehler: NoData (unvollständig), ProtocolError (Magic/Version/
    /// Kind/Länge), ChecksumError (CRC).
    virtual Status decode(const std::uint8_t* input, std::size_t size,
                          MessageEnvelope& envelope,
                          std::size_t& consumed) const noexcept = 0;
};

}  // namespace protocol
}  // namespace mea
