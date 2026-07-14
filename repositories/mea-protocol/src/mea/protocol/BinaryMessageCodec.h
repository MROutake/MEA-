#pragma once

/// @file BinaryMessageCodec.h
/// @brief Binärer Encoder/Decoder für das MEA-Netzprotokoll (little-endian,
///        CRC-16/CCITT, siehe PROTOCOL-SPEC). Keine Allokation, kein Zustand.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IMessageCodec.h"
#include "MessageEnvelope.h"

namespace mea {
namespace protocol {

class BinaryMessageCodec final : public IMessageEncoder, public IMessageDecoder {
public:
    Status encode(const MessageEnvelope& envelope, std::uint8_t* output,
                  std::size_t capacity, std::size_t& written) const noexcept override;

    Status decode(const std::uint8_t* input, std::size_t size,
                  MessageEnvelope& envelope,
                  std::size_t& consumed) const noexcept override;

private:
    /// Schreibt den Payload je Kind ab @p offset. Erwartet, dass der Puffer
    /// groß genug ist (Aufrufer stellt Kapazität sicher).
    static void encodePayload(const MessageEnvelope& envelope, std::uint8_t* output,
                              std::size_t& offset) noexcept;

    /// Liest den Payload je Kind ab @p offset in die Envelope-Union.
    static void decodePayload(MessageKind kind, const std::uint8_t* input,
                              std::size_t& offset, MessageEnvelope& envelope) noexcept;
};

}  // namespace protocol
}  // namespace mea
