#include "BinaryMessageCodec.h"

#include "ByteOrder.h"
#include "Crc16.h"
#include "MessageHeader.h"
#include "MessageKind.h"

namespace mea {
namespace protocol {

namespace {

void encodeHeader(const MessageHeader& header, const MessageKind kind,
                  const std::uint16_t payloadLength, std::uint8_t* output,
                  std::size_t& offset) noexcept {
    le::writeU16(output, offset, kFrameMagic);
    le::writeU8(output, offset, kProtocolVersion);
    le::writeU8(output, offset, static_cast<std::uint8_t>(kind));
    le::writeU16(output, offset, header.componentId);
    le::writeU16(output, offset, header.targetId);
    le::writeU32(output, offset, header.sequence);
    le::writeU32(output, offset, header.timestampMs);
    le::writeU16(output, offset, payloadLength);
}

}  // namespace

void BinaryMessageCodec::encodePayload(const MessageEnvelope& envelope,
                                       std::uint8_t* output,
                                       std::size_t& offset) noexcept {
    switch (envelope.header.kind) {
        case MessageKind::Measurement: {
            const MeasurementPayload& m = envelope.payload.measurement;
            le::writeU16(output, offset, m.sourceId);
            le::writeU8(output, offset, static_cast<std::uint8_t>(m.kind));
            le::writeU8(output, offset, static_cast<std::uint8_t>(m.unit));
            le::writeF32(output, offset, m.value);
            le::writeU32(output, offset, m.sampledAtMs);
            le::writeU32(output, offset, m.sequence);
            le::writeU16(output, offset, static_cast<std::uint16_t>(m.quality));
            break;
        }
        case MessageKind::OutputState: {
            const OutputStatePayload& o = envelope.payload.outputState;
            le::writeU16(output, offset, o.componentId);
            le::writeU8(output, offset, o.channelCount);
            le::writeU8(output, offset, o.byteCount);
            for (std::size_t index = 0; index < kMaxOutputStateBytes; ++index) {
                le::writeU8(output, offset, o.state[index]);
            }
            le::writeU32(output, offset, o.appliedAtMs);
            break;
        }
        case MessageKind::StateTransition: {
            const StateTransitionPayload& s = envelope.payload.stateTransition;
            le::writeU16(output, offset, s.componentId);
            le::writeU8(output, offset, s.fromState);
            le::writeU8(output, offset, s.toState);
            le::writeU16(output, offset, s.reason);
            le::writeU16(output, offset, s.flags);
            le::writeU32(output, offset, s.atMs);
            break;
        }
        case MessageKind::ErrorEvent: {
            const ErrorEventPayload& e = envelope.payload.errorEvent;
            le::writeU16(output, offset, e.componentId);
            le::writeU8(output, offset, e.code);
            le::writeU8(output, offset, e.severity);
            le::writeU16(output, offset, e.detail);
            le::writeU16(output, offset, e.flags);
            le::writeU32(output, offset, e.atMs);
            break;
        }
        case MessageKind::Heartbeat: {
            const HeartbeatPayload& h = envelope.payload.heartbeat;
            le::writeU16(output, offset, h.componentId);
            le::writeU32(output, offset, h.uptimeMs);
            le::writeU32(output, offset, h.sequence);
            le::writeU16(output, offset, h.flags);
            le::writeU8(output, offset, h.state);
            le::writeU8(output, offset, h.reserved);
            break;
        }
        case MessageKind::Command: {
            const CommandPayload& c = envelope.payload.command;
            le::writeU16(output, offset, c.sourceId);
            le::writeU16(output, offset, c.targetId);
            le::writeU16(output, offset, c.type);
            le::writeU32(output, offset, c.argument);
            le::writeU32(output, offset, c.atMs);
            break;
        }
        case MessageKind::Unknown:
            break;
    }
}

void BinaryMessageCodec::decodePayload(const MessageKind kind,
                                       const std::uint8_t* input, std::size_t& offset,
                                       MessageEnvelope& envelope) noexcept {
    switch (kind) {
        case MessageKind::Measurement: {
            MeasurementPayload m{};
            m.sourceId = le::readU16(input, offset);
            m.kind = static_cast<MeasurementKind>(le::readU8(input, offset));
            m.unit = static_cast<Unit>(le::readU8(input, offset));
            m.value = le::readF32(input, offset);
            m.sampledAtMs = le::readU32(input, offset);
            m.sequence = le::readU32(input, offset);
            m.quality = static_cast<QualityFlag>(le::readU16(input, offset));
            envelope.payload.measurement = m;
            break;
        }
        case MessageKind::OutputState: {
            OutputStatePayload o{};
            o.componentId = le::readU16(input, offset);
            o.channelCount = le::readU8(input, offset);
            o.byteCount = le::readU8(input, offset);
            for (std::size_t index = 0; index < kMaxOutputStateBytes; ++index) {
                o.state[index] = le::readU8(input, offset);
            }
            o.appliedAtMs = le::readU32(input, offset);
            envelope.payload.outputState = o;
            break;
        }
        case MessageKind::StateTransition: {
            StateTransitionPayload s{};
            s.componentId = le::readU16(input, offset);
            s.fromState = le::readU8(input, offset);
            s.toState = le::readU8(input, offset);
            s.reason = le::readU16(input, offset);
            s.flags = le::readU16(input, offset);
            s.atMs = le::readU32(input, offset);
            envelope.payload.stateTransition = s;
            break;
        }
        case MessageKind::ErrorEvent: {
            ErrorEventPayload e{};
            e.componentId = le::readU16(input, offset);
            e.code = le::readU8(input, offset);
            e.severity = le::readU8(input, offset);
            e.detail = le::readU16(input, offset);
            e.flags = le::readU16(input, offset);
            e.atMs = le::readU32(input, offset);
            envelope.payload.errorEvent = e;
            break;
        }
        case MessageKind::Heartbeat: {
            HeartbeatPayload h{};
            h.componentId = le::readU16(input, offset);
            h.uptimeMs = le::readU32(input, offset);
            h.sequence = le::readU32(input, offset);
            h.flags = le::readU16(input, offset);
            h.state = le::readU8(input, offset);
            h.reserved = le::readU8(input, offset);
            envelope.payload.heartbeat = h;
            break;
        }
        case MessageKind::Command: {
            CommandPayload c{};
            c.sourceId = le::readU16(input, offset);
            c.targetId = le::readU16(input, offset);
            c.type = le::readU16(input, offset);
            c.argument = le::readU32(input, offset);
            c.atMs = le::readU32(input, offset);
            envelope.payload.command = c;
            break;
        }
        case MessageKind::Unknown:
            break;
    }
}

Status BinaryMessageCodec::encode(const MessageEnvelope& envelope, std::uint8_t* output,
                                  const std::size_t capacity,
                                  std::size_t& written) const noexcept {
    written = 0;
    if (output == nullptr) {
        return makeStatus(StatusCode::InvalidArgument, envelope.header.componentId);
    }
    const MessageKind kind = envelope.header.kind;
    if (!isKnownKind(kind)) {
        return makeStatus(StatusCode::InvalidArgument, envelope.header.componentId);
    }

    const std::size_t payloadLength = payloadLengthFor(kind);
    const std::size_t frameLength = kHeaderSize + payloadLength + kCrcSize;
    if (capacity < frameLength) {
        return makeStatus(StatusCode::CapacityExceeded, envelope.header.componentId,
                          static_cast<std::uint16_t>(frameLength));
    }

    std::size_t offset = 0;
    encodeHeader(envelope.header, kind, static_cast<std::uint16_t>(payloadLength), output,
                 offset);
    encodePayload(envelope, output, offset);

    const std::uint16_t crc = crc16(output, kHeaderSize + payloadLength);
    le::writeU16(output, offset, crc);

    written = frameLength;
    return okStatus();
}

Status BinaryMessageCodec::decode(const std::uint8_t* input, const std::size_t size,
                                  MessageEnvelope& envelope,
                                  std::size_t& consumed) const noexcept {
    consumed = 0;
    if (input == nullptr) {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
    }
    if (size < kHeaderSize) {
        return makeStatus(StatusCode::NoData, InvalidComponentId);
    }

    std::size_t offset = 0;
    const std::uint16_t magic = le::readU16(input, offset);
    if (magic != kFrameMagic) {
        consumed = 1;  // um ein Byte weiterrücken und neu synchronisieren
        return makeStatus(StatusCode::ProtocolError, InvalidComponentId);
    }
    const std::uint8_t version = le::readU8(input, offset);
    if (version != kProtocolVersion) {
        consumed = 1;
        return makeStatus(StatusCode::ProtocolError, InvalidComponentId, version);
    }
    const MessageKind kind = static_cast<MessageKind>(le::readU8(input, offset));
    if (!isKnownKind(kind)) {
        consumed = 1;
        return makeStatus(StatusCode::ProtocolError, InvalidComponentId);
    }

    MessageHeader header{};
    header.magic = magic;
    header.protocolVersion = version;
    header.kind = kind;
    header.componentId = le::readU16(input, offset);
    header.targetId = le::readU16(input, offset);
    header.sequence = le::readU32(input, offset);
    header.timestampMs = le::readU32(input, offset);
    header.payloadLength = le::readU16(input, offset);

    const std::size_t expected = payloadLengthFor(kind);
    if (header.payloadLength != expected) {
        consumed = 1;  // Länge inkonsistent -> resynchronisieren
        return makeStatus(StatusCode::ProtocolError, header.componentId,
                          header.payloadLength);
    }

    const std::size_t frameLength = kHeaderSize + expected + kCrcSize;
    if (size < frameLength) {
        return makeStatus(StatusCode::NoData, header.componentId);  // consumed = 0
    }

    // CRC über Header + Payload prüfen, bevor der Payload interpretiert wird.
    std::size_t crcOffset = kHeaderSize + expected;
    const std::uint16_t expectedCrc = crc16(input, crcOffset);
    const std::uint16_t actualCrc = le::readU16(input, crcOffset);
    if (expectedCrc != actualCrc) {
        consumed = frameLength;  // ganzen erkannten Frame verwerfen
        return makeStatus(StatusCode::ChecksumError, header.componentId);
    }

    envelope.header = header;
    decodePayload(kind, input, offset, envelope);
    consumed = frameLength;
    return okStatus();
}

}  // namespace protocol
}  // namespace mea
