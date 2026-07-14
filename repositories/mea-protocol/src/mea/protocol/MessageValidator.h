#pragma once

/// @file MessageValidator.h
/// @brief Strukturvalidierung von Headern/Envelopes unabhängig vom Transport
///        (siehe PROTOCOL-SPEC §6). Ergänzt die CRC-Prüfung des Decoders um
///        semantische Regeln (Version, Kind, Länge, gültige Komponenten-ID).

#include <MeaCore.h>

#include "MessageEnvelope.h"
#include "MessageHeader.h"
#include "MessageKind.h"

namespace mea {
namespace protocol {

class MessageValidator {
public:
    /// Prüft Magic, Version, Kind, Payload-Länge und Komponenten-ID.
    [[nodiscard]] static Status validateHeader(const MessageHeader& header) noexcept {
        if (header.magic != kFrameMagic) {
            return makeStatus(StatusCode::ProtocolError, header.componentId);
        }
        if (header.protocolVersion != kProtocolVersion) {
            return makeStatus(StatusCode::ProtocolError, header.componentId,
                              header.protocolVersion);
        }
        if (!isKnownKind(header.kind)) {
            return makeStatus(StatusCode::ProtocolError, header.componentId,
                              static_cast<std::uint16_t>(header.kind));
        }
        if (header.payloadLength != payloadLengthFor(header.kind)) {
            return makeStatus(StatusCode::ProtocolError, header.componentId,
                              header.payloadLength);
        }
        if (header.componentId == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
        }
        return okStatus();
    }

    /// Prüft die Envelope (aktuell = Header-Validierung).
    [[nodiscard]] static Status validateEnvelope(
        const MessageEnvelope& envelope) noexcept {
        return validateHeader(envelope.header);
    }
};

}  // namespace protocol
}  // namespace mea
