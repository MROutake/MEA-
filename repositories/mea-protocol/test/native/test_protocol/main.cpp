#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <MeaProtocol.h>

void setUp() {}
void tearDown() {}

using namespace mea;
using namespace mea::protocol;

namespace {

// ---- Golden frames (implementierungsunabhaengig, siehe PROTOCOL-SPEC §8) ----

// Measurement: comp=610, target=700, seq=42, ts=12345, value=1.25,
// kind=Voltage(2), unit=Volt(2), quality=0.
const std::uint8_t kGoldenMeasurement[] = {
    0x4D, 0x41, 0x01, 0x01, 0x62, 0x02, 0xBC, 0x02, 0x2A, 0x00, 0x00, 0x00, 0x39,
    0x30, 0x00, 0x00, 0x12, 0x00, 0x62, 0x02, 0x02, 0x02, 0x00, 0x00, 0xA0, 0x3F,
    0x39, 0x30, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEA, 0x67};

// Heartbeat: comp=515, target=0, seq=7, ts=1234, uptime=256, hbSeq=7,
// flags=0, state=1.
const std::uint8_t kGoldenHeartbeat[] = {
    0x4D, 0x41, 0x01, 0x05, 0x03, 0x02, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0xD2, 0x04, 0x00, 0x00, 0x0E, 0x00, 0x03, 0x02, 0x00, 0x01, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xA2, 0xA9};

MeasurementPayload sampleMeasurement() {
    MeasurementPayload m{};
    m.sourceId = 610;
    m.kind = MeasurementKind::Voltage;
    m.unit = Unit::Volt;
    m.value = 1.25F;
    m.sampledAtMs = 12345;
    m.sequence = 42;
    m.quality = QualityFlag::None;
    return m;
}

}  // namespace

// -------------------------------------------------------------- Golden frames

static void test_encode_matches_golden_measurement() {
    const BinaryMessageCodec codec;
    const MessageEnvelope envelope =
        makeMeasurementEnvelope(610, 700, 42, 12345, sampleMeasurement());
    std::uint8_t buffer[64] = {};
    std::size_t written = 0;
    TEST_ASSERT_TRUE(codec.encode(envelope, buffer, sizeof(buffer), written).ok());
    TEST_ASSERT_EQUAL_size_t(sizeof(kGoldenMeasurement), written);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kGoldenMeasurement, buffer, sizeof(kGoldenMeasurement));
}

static void test_encode_matches_golden_heartbeat() {
    const BinaryMessageCodec codec;
    HeartbeatPayload hb{};
    hb.componentId = 515;
    hb.uptimeMs = 256;
    hb.sequence = 7;
    hb.flags = 0;
    hb.state = 1;
    const MessageEnvelope envelope =
        makeHeartbeatEnvelope(515, kBroadcastTarget, 7, 1234, hb);
    std::uint8_t buffer[64] = {};
    std::size_t written = 0;
    TEST_ASSERT_TRUE(codec.encode(envelope, buffer, sizeof(buffer), written).ok());
    TEST_ASSERT_EQUAL_size_t(sizeof(kGoldenHeartbeat), written);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kGoldenHeartbeat, buffer, sizeof(kGoldenHeartbeat));
}

static void test_decode_golden_measurement() {
    const BinaryMessageCodec codec;
    MessageEnvelope envelope{};
    std::size_t consumed = 0;
    const Status status = codec.decode(kGoldenMeasurement, sizeof(kGoldenMeasurement),
                                       envelope, consumed);
    TEST_ASSERT_TRUE(status.ok());
    TEST_ASSERT_EQUAL_size_t(sizeof(kGoldenMeasurement), consumed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MessageKind::Measurement),
                            static_cast<std::uint8_t>(envelope.header.kind));
    TEST_ASSERT_EQUAL_UINT16(610, envelope.header.componentId);
    TEST_ASSERT_EQUAL_UINT16(700, envelope.header.targetId);
    TEST_ASSERT_EQUAL_UINT32(42, envelope.header.sequence);
    TEST_ASSERT_EQUAL_UINT32(12345, envelope.header.timestampMs);
    TEST_ASSERT_EQUAL_FLOAT(1.25F, envelope.payload.measurement.value);
    TEST_ASSERT_EQUAL_UINT16(610, envelope.payload.measurement.sourceId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MeasurementKind::Voltage),
                            static_cast<std::uint8_t>(envelope.payload.measurement.kind));
}

// -------------------------------------------------------------- Roundtrips

// Encodiert und decodiert @p in und prueft den Header. Payload-Felder werden je
// Test einzeln verglichen (keine rohen Union-/Padding-Bytes vergleichen).
static MessageEnvelope encodeDecode(const MessageEnvelope& in) {
    const BinaryMessageCodec codec;
    std::uint8_t buffer[kMaxFrameSize] = {};
    std::size_t written = 0;
    TEST_ASSERT_TRUE(codec.encode(in, buffer, sizeof(buffer), written).ok());

    MessageEnvelope out{};
    std::size_t consumed = 0;
    TEST_ASSERT_TRUE(codec.decode(buffer, written, out, consumed).ok());
    TEST_ASSERT_EQUAL_size_t(written, consumed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(in.header.kind),
                            static_cast<std::uint8_t>(out.header.kind));
    TEST_ASSERT_EQUAL_UINT16(in.header.componentId, out.header.componentId);
    TEST_ASSERT_EQUAL_UINT16(in.header.targetId, out.header.targetId);
    TEST_ASSERT_EQUAL_UINT32(in.header.sequence, out.header.sequence);
    TEST_ASSERT_EQUAL_UINT32(in.header.timestampMs, out.header.timestampMs);
    return out;
}

static void test_roundtrip_measurement() {
    const MessageEnvelope out = encodeDecode(
        makeMeasurementEnvelope(610, 700, 42, 12345, sampleMeasurement()));
    const MeasurementPayload& m = out.payload.measurement;
    TEST_ASSERT_EQUAL_UINT16(610, m.sourceId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MeasurementKind::Voltage),
                            static_cast<std::uint8_t>(m.kind));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(Unit::Volt),
                            static_cast<std::uint8_t>(m.unit));
    TEST_ASSERT_EQUAL_FLOAT(1.25F, m.value);
    TEST_ASSERT_EQUAL_UINT32(12345, m.sampledAtMs);
    TEST_ASSERT_EQUAL_UINT32(42, m.sequence);
}

static void test_roundtrip_output_state() {
    OutputStatePayload o{};
    o.componentId = 620;
    o.channelCount = 16;
    o.byteCount = 2;
    o.state[0] = 0xA5;
    o.state[1] = 0x3C;
    o.appliedAtMs = 999;
    const MessageEnvelope out = encodeDecode(makeOutputStateEnvelope(620, 0, 1, 999, o));
    const OutputStatePayload& r = out.payload.outputState;
    TEST_ASSERT_EQUAL_UINT16(620, r.componentId);
    TEST_ASSERT_EQUAL_UINT8(16, r.channelCount);
    TEST_ASSERT_EQUAL_UINT8(2, r.byteCount);
    TEST_ASSERT_EQUAL_HEX8(0xA5, r.state[0]);
    TEST_ASSERT_EQUAL_HEX8(0x3C, r.state[1]);
    TEST_ASSERT_EQUAL_UINT32(999, r.appliedAtMs);
}

static void test_roundtrip_state_transition() {
    StateTransitionPayload s{};
    s.componentId = 630;
    s.fromState = 3;
    s.toState = 4;
    s.reason = 7;
    s.atMs = 555;
    const MessageEnvelope out =
        encodeDecode(makeStateTransitionEnvelope(630, 0, 2, 555, s));
    const StateTransitionPayload& r = out.payload.stateTransition;
    TEST_ASSERT_EQUAL_UINT16(630, r.componentId);
    TEST_ASSERT_EQUAL_UINT8(3, r.fromState);
    TEST_ASSERT_EQUAL_UINT8(4, r.toState);
    TEST_ASSERT_EQUAL_UINT16(7, r.reason);
    TEST_ASSERT_EQUAL_UINT32(555, r.atMs);
}

static void test_roundtrip_error_event() {
    ErrorEventPayload e{};
    e.componentId = 640;
    e.code = static_cast<std::uint8_t>(StatusCode::Timeout);
    e.severity = 2;
    e.detail = 12;
    e.atMs = 321;
    const MessageEnvelope out = encodeDecode(makeErrorEventEnvelope(640, 0, 3, 321, e));
    const ErrorEventPayload& r = out.payload.errorEvent;
    TEST_ASSERT_EQUAL_UINT16(640, r.componentId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::Timeout), r.code);
    TEST_ASSERT_EQUAL_UINT8(2, r.severity);
    TEST_ASSERT_EQUAL_UINT16(12, r.detail);
    TEST_ASSERT_EQUAL_UINT32(321, r.atMs);
}

static void test_roundtrip_heartbeat() {
    HeartbeatPayload h{};
    h.componentId = 515;
    h.uptimeMs = 123456;
    h.sequence = 9;
    h.flags = 0x00FF;
    h.state = 1;
    const MessageEnvelope out = encodeDecode(makeHeartbeatEnvelope(515, 0, 9, 1000, h));
    const HeartbeatPayload& r = out.payload.heartbeat;
    TEST_ASSERT_EQUAL_UINT16(515, r.componentId);
    TEST_ASSERT_EQUAL_UINT32(123456, r.uptimeMs);
    TEST_ASSERT_EQUAL_UINT32(9, r.sequence);
    TEST_ASSERT_EQUAL_UINT16(0x00FF, r.flags);
    TEST_ASSERT_EQUAL_UINT8(1, r.state);
}

static void test_roundtrip_command() {
    CommandPayload c{};
    c.sourceId = 500;
    c.targetId = 630;
    c.type = static_cast<std::uint16_t>(CommandType::Start);
    c.argument = 0xDEADBEEF;
    c.atMs = 77;
    const MessageEnvelope out = encodeDecode(makeCommandEnvelope(500, 630, 4, 77, c));
    const CommandPayload& r = out.payload.command;
    TEST_ASSERT_EQUAL_UINT16(500, r.sourceId);
    TEST_ASSERT_EQUAL_UINT16(630, r.targetId);
    TEST_ASSERT_EQUAL_UINT16(static_cast<std::uint16_t>(CommandType::Start), r.type);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, r.argument);
    TEST_ASSERT_EQUAL_UINT32(77, r.atMs);
}

// -------------------------------------------------------------- Fehlerfaelle

static void test_encode_rejects_unknown_kind() {
    const BinaryMessageCodec codec;
    MessageEnvelope envelope{};  // kind = Unknown
    std::uint8_t buffer[64] = {};
    std::size_t written = 0;
    const Status status = codec.encode(envelope, buffer, sizeof(buffer), written);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::InvalidArgument),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_size_t(0, written);
}

static void test_encode_reports_capacity_exceeded() {
    const BinaryMessageCodec codec;
    const MessageEnvelope envelope =
        makeMeasurementEnvelope(610, 0, 1, 1, sampleMeasurement());
    std::uint8_t buffer[8] = {};
    std::size_t written = 0;
    const Status status = codec.encode(envelope, buffer, sizeof(buffer), written);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::CapacityExceeded),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(kHeaderSize + 18U + kCrcSize, status.detail);
    TEST_ASSERT_EQUAL_size_t(0, written);
}

static void test_decode_incomplete_reports_no_data() {
    const BinaryMessageCodec codec;
    MessageEnvelope envelope{};
    std::size_t consumed = 0;
    // Header vollstaendig, Payload fehlt.
    const Status status = codec.decode(kGoldenMeasurement, kHeaderSize, envelope, consumed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::NoData),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_size_t(0, consumed);
}

static void test_decode_bad_magic_resyncs_by_one() {
    const BinaryMessageCodec codec;
    std::uint8_t frame[sizeof(kGoldenHeartbeat)];
    std::memcpy(frame, kGoldenHeartbeat, sizeof(frame));
    frame[0] = 0x00;  // Magic zerstoeren
    MessageEnvelope envelope{};
    std::size_t consumed = 0;
    const Status status = codec.decode(frame, sizeof(frame), envelope, consumed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::ProtocolError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_size_t(1, consumed);
}

static void test_decode_bad_version_reports_protocol_error() {
    const BinaryMessageCodec codec;
    std::uint8_t frame[sizeof(kGoldenHeartbeat)];
    std::memcpy(frame, kGoldenHeartbeat, sizeof(frame));
    frame[2] = 0x99;  // Version zerstoeren
    MessageEnvelope envelope{};
    std::size_t consumed = 0;
    const Status status = codec.decode(frame, sizeof(frame), envelope, consumed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::ProtocolError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(0x99, status.detail);
    TEST_ASSERT_EQUAL_size_t(1, consumed);
}

static void test_decode_corrupt_crc_drops_frame() {
    const BinaryMessageCodec codec;
    std::uint8_t frame[sizeof(kGoldenHeartbeat)];
    std::memcpy(frame, kGoldenHeartbeat, sizeof(frame));
    frame[20] ^= 0xFFU;  // Payload-Byte kippen -> CRC passt nicht mehr
    MessageEnvelope envelope{};
    std::size_t consumed = 0;
    const Status status = codec.decode(frame, sizeof(frame), envelope, consumed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::ChecksumError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_size_t(sizeof(frame), consumed);  // ganzer Frame verworfen
}

static void test_decode_inconsistent_length_reports_protocol_error() {
    const BinaryMessageCodec codec;
    std::uint8_t frame[sizeof(kGoldenHeartbeat)];
    std::memcpy(frame, kGoldenHeartbeat, sizeof(frame));
    frame[16] = 0x20;  // payloadLength (LE low byte) = 32 statt 14
    MessageEnvelope envelope{};
    std::size_t consumed = 0;
    const Status status = codec.decode(frame, sizeof(frame), envelope, consumed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::ProtocolError),
                            static_cast<std::uint8_t>(status.code));
}

// -------------------------------------------------------------- Validator

static void test_validator_accepts_valid_header() {
    const MessageEnvelope envelope =
        makeMeasurementEnvelope(610, 0, 1, 1, sampleMeasurement());
    TEST_ASSERT_TRUE(MessageValidator::validateEnvelope(envelope).ok());
}

static void test_validator_rejects_invalid_component_id() {
    MessageEnvelope envelope =
        makeMeasurementEnvelope(610, 0, 1, 1, sampleMeasurement());
    envelope.header.componentId = InvalidComponentId;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(StatusCode::InvalidArgument),
        static_cast<std::uint8_t>(MessageValidator::validateEnvelope(envelope).code));
}

static void test_validator_rejects_bad_version() {
    MessageEnvelope envelope =
        makeMeasurementEnvelope(610, 0, 1, 1, sampleMeasurement());
    envelope.header.protocolVersion = 42;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(StatusCode::ProtocolError),
        static_cast<std::uint8_t>(MessageValidator::validateEnvelope(envelope).code));
}

// -------------------------------------------------------------- Registry

static void test_registry_add_find_allows() {
    ComponentRegistry<4> registry;
    const std::uint16_t mask = static_cast<std::uint16_t>(kindBit(MessageKind::Measurement) |
                                                          kindBit(MessageKind::Heartbeat));
    TEST_ASSERT_TRUE(registry.add(610, mask).ok());
    TEST_ASSERT_TRUE(registry.contains(610));
    TEST_ASSERT_FALSE(registry.contains(999));
    TEST_ASSERT_TRUE(registry.allows(610, MessageKind::Measurement));
    TEST_ASSERT_TRUE(registry.allows(610, MessageKind::Heartbeat));
    TEST_ASSERT_FALSE(registry.allows(610, MessageKind::Command));
    TEST_ASSERT_FALSE(registry.allows(999, MessageKind::Measurement));
}

static void test_registry_rejects_duplicate_and_invalid() {
    ComponentRegistry<2> registry;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(StatusCode::InvalidArgument),
        static_cast<std::uint8_t>(registry.add(InvalidComponentId, 0).code));
    TEST_ASSERT_TRUE(registry.add(1, 0).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::DuplicateId),
                            static_cast<std::uint8_t>(registry.add(1, 0).code));
    TEST_ASSERT_TRUE(registry.add(2, 0).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::CapacityExceeded),
                            static_cast<std::uint8_t>(registry.add(3, 0).code));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_matches_golden_measurement);
    RUN_TEST(test_encode_matches_golden_heartbeat);
    RUN_TEST(test_decode_golden_measurement);
    RUN_TEST(test_roundtrip_measurement);
    RUN_TEST(test_roundtrip_output_state);
    RUN_TEST(test_roundtrip_state_transition);
    RUN_TEST(test_roundtrip_error_event);
    RUN_TEST(test_roundtrip_heartbeat);
    RUN_TEST(test_roundtrip_command);
    RUN_TEST(test_encode_rejects_unknown_kind);
    RUN_TEST(test_encode_reports_capacity_exceeded);
    RUN_TEST(test_decode_incomplete_reports_no_data);
    RUN_TEST(test_decode_bad_magic_resyncs_by_one);
    RUN_TEST(test_decode_bad_version_reports_protocol_error);
    RUN_TEST(test_decode_corrupt_crc_drops_frame);
    RUN_TEST(test_decode_inconsistent_length_reports_protocol_error);
    RUN_TEST(test_validator_accepts_valid_header);
    RUN_TEST(test_validator_rejects_invalid_component_id);
    RUN_TEST(test_validator_rejects_bad_version);
    RUN_TEST(test_registry_add_find_allows);
    RUN_TEST(test_registry_rejects_duplicate_and_invalid);
    return UNITY_END();
}
