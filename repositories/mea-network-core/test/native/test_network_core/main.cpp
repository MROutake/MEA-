#include <unity.h>

#include <cstddef>
#include <cstdint>

#include <MeaNetworkCore.h>
#include <MeaProtocol.h>
#include <mea/network/testing/FakeNetworkTransport.h>

void setUp() {}
void tearDown() {}

using namespace mea;
using namespace mea::network;
using namespace mea::network::testing;
using mea::protocol::BinaryMessageCodec;
using mea::protocol::MessageEnvelope;
using mea::protocol::MessageKind;

namespace {

constexpr ComponentId kTransportId = 800;
constexpr ComponentId kBridgeSinkId = 810;

Measurement sampleMeasurement() {
    Measurement m{};
    m.sourceId = 610;
    m.kind = MeasurementKind::Voltage;
    m.unit = Unit::Volt;
    m.value = 3.3F;
    m.sampledAtMs = 4242;
    m.sequence = 7;
    return m;
}

ReconnectPolicy fastPolicy(std::uint16_t maxAttempts = 0) {
    ReconnectPolicy policy{};
    policy.initialBackoffMs = 10;
    policy.maxBackoffMs = 100;
    policy.backoffMultiplier = 2;
    policy.maxAttempts = maxAttempts;
    policy.connectTimeoutMs = 100;
    return policy;
}

}  // namespace

// -------------------------------------------------------------- ReconnectPolicy

static void test_backoff_grows_and_caps() {
    const ReconnectPolicy policy = fastPolicy();
    const TimestampMs b1 = nextBackoff(policy, 0);    // -> initial 10
    const TimestampMs b2 = nextBackoff(policy, b1);   // -> 20
    const TimestampMs b3 = nextBackoff(policy, b2);   // -> 40
    const TimestampMs b4 = nextBackoff(policy, 80);   // -> capped 100
    TEST_ASSERT_EQUAL_UINT32(10, b1);
    TEST_ASSERT_EQUAL_UINT32(20, b2);
    TEST_ASSERT_EQUAL_UINT32(40, b3);
    TEST_ASSERT_EQUAL_UINT32(100, b4);
}

// -------------------------------------------------------------- NetworkSession

static void test_session_connects_to_online() {
    FakeNetworkTransport transport(kTransportId);
    transport.connectDelayMs = 10;
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);

    TEST_ASSERT_TRUE(session.begin(0).ok());
    TEST_ASSERT_TRUE(session.connect(0).ok());
    TEST_ASSERT_EQUAL(NetworkSession::State::Connecting, session.state());
    session.update(5);
    TEST_ASSERT_EQUAL(NetworkSession::State::Connecting, session.state());
    session.update(10);
    TEST_ASSERT_EQUAL(NetworkSession::State::Online, session.state());
    TEST_ASSERT_TRUE(session.isOnline());
    TEST_ASSERT_EQUAL_UINT32(1, metrics.connectAttempts);
}

static void test_session_failed_connect_backs_off_and_retries() {
    FakeNetworkTransport transport(kTransportId);
    transport.failConnect = true;
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    TEST_ASSERT_TRUE(session.begin(0).ok());

    session.connect(0);          // attempt 1 -> Error erkannt in update
    session.update(0);
    TEST_ASSERT_EQUAL(NetworkSession::State::Backoff, session.state());

    // Nach Backoff (10 ms) neuer Versuch; jetzt gelingt die Verbindung.
    transport.failConnect = false;
    transport.connectDelayMs = 0;
    session.update(10);          // Backoff abgelaufen -> Connecting
    TEST_ASSERT_EQUAL(NetworkSession::State::Connecting, session.state());
    session.update(10);          // transport wird Up -> Online
    TEST_ASSERT_EQUAL(NetworkSession::State::Online, session.state());
    TEST_ASSERT_EQUAL_UINT32(2, metrics.connectAttempts);
}

static void test_session_faults_after_max_attempts() {
    FakeNetworkTransport transport(kTransportId);
    transport.failConnect = true;
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(2), metrics, 801);
    TEST_ASSERT_TRUE(session.begin(0).ok());

    session.connect(0);   // attempt 1
    session.update(0);    // Error -> Backoff
    TEST_ASSERT_EQUAL(NetworkSession::State::Backoff, session.state());
    session.update(20);   // Backoff -> attempt 2
    session.update(20);   // Error -> maxAttempts erreicht -> Fault
    TEST_ASSERT_EQUAL(NetworkSession::State::Fault, session.state());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::IoError),
                            static_cast<std::uint8_t>(session.lastStatus().code));
}

static void test_session_connect_timeout() {
    FakeNetworkTransport transport(kTransportId);
    transport.connectDelayMs = 100000;  // wird nie Up
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    TEST_ASSERT_TRUE(session.begin(0).ok());
    session.connect(0);
    session.update(50);
    TEST_ASSERT_EQUAL(NetworkSession::State::Connecting, session.state());
    session.update(100);  // connectTimeoutMs = 100 erreicht
    TEST_ASSERT_EQUAL(NetworkSession::State::Backoff, session.state());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::Timeout),
                            static_cast<std::uint8_t>(session.lastStatus().code));
}

static void test_session_reconnects_after_link_loss() {
    FakeNetworkTransport transport(kTransportId);
    transport.connectDelayMs = 0;
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    TEST_ASSERT_TRUE(session.begin(0).ok());
    session.connect(0);
    session.update(0);
    TEST_ASSERT_TRUE(session.isOnline());

    transport.dropLink();
    session.update(1);
    TEST_ASSERT_EQUAL(NetworkSession::State::Backoff, session.state());
    TEST_ASSERT_EQUAL_UINT32(1, metrics.reconnectCount);
}

static void test_session_disconnect_stops_reconnect() {
    FakeNetworkTransport transport(kTransportId);
    transport.connectDelayMs = 0;
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    TEST_ASSERT_TRUE(session.begin(0).ok());
    session.connect(0);
    session.update(0);
    TEST_ASSERT_TRUE(session.isOnline());
    session.disconnect();
    TEST_ASSERT_EQUAL(NetworkSession::State::Disconnected, session.state());
    session.update(100);
    TEST_ASSERT_EQUAL(NetworkSession::State::Disconnected, session.state());
}

// -------------------------------------------------------------- ProtocolBridge

using Bridge = ProtocolBridge<512, 512, 4>;

static void bringOnline(NetworkSession& session, FakeNetworkTransport& transport) {
    transport.connectDelayMs = 0;
    TEST_ASSERT_TRUE(session.begin(0).ok());
    TEST_ASSERT_TRUE(session.connect(0).ok());
    session.update(0);
    TEST_ASSERT_TRUE(session.isOnline());
}

static void test_bridge_loopback_roundtrip() {
    FakeNetworkTransport transport(kTransportId);
    transport.loopback = true;
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    const BinaryMessageCodec codec;
    Bridge bridge(session, transport, codec, codec, metrics);

    bringOnline(session, transport);
    TEST_ASSERT_TRUE(bridge.begin().ok());

    const MessageEnvelope out =
        mea::protocol::makeMeasurementEnvelope(610, 0, 1, 4242, sampleMeasurement());
    TEST_ASSERT_TRUE(bridge.publish(out).ok());
    TEST_ASSERT_EQUAL_UINT32(1, metrics.txFrameCount);

    TEST_ASSERT_TRUE(bridge.update(0).ok());  // TX->transport->(loopback)->RX->decode

    MessageEnvelope in{};
    TEST_ASSERT_TRUE(bridge.poll(in).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MessageKind::Measurement),
                            static_cast<std::uint8_t>(in.header.kind));
    TEST_ASSERT_EQUAL_FLOAT(3.3F, in.payload.measurement.value);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.rxFrameCount);
    TEST_ASSERT_EQUAL_size_t(0, bridge.txPending());
}

static void test_bridge_full_tx_drops_and_reports_would_block() {
    FakeNetworkTransport transport(kTransportId);  // bleibt offline -> kein Abfluss
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    const BinaryMessageCodec codec;
    ProtocolBridge<mea::protocol::kMaxFrameSize, 512, 2> bridge(session, transport, codec,
                                                                codec, metrics);
    TEST_ASSERT_TRUE(session.begin(0).ok());
    TEST_ASSERT_TRUE(bridge.begin().ok());

    const MessageEnvelope env =
        mea::protocol::makeMeasurementEnvelope(610, 0, 1, 4242, sampleMeasurement());
    bool sawWouldBlock = false;
    for (int i = 0; i < 50 && !sawWouldBlock; ++i) {
        const Status status = bridge.publish(env);
        if (status.code == StatusCode::WouldBlock) {
            sawWouldBlock = true;
        }
    }
    TEST_ASSERT_TRUE(sawWouldBlock);
    TEST_ASSERT_GREATER_THAN_UINT32(0, metrics.txDropCount);
}

static void test_bridge_rejects_unknown_kind() {
    FakeNetworkTransport transport(kTransportId);
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    const BinaryMessageCodec codec;
    Bridge bridge(session, transport, codec, codec, metrics);
    TEST_ASSERT_TRUE(session.begin(0).ok());
    TEST_ASSERT_TRUE(bridge.begin().ok());

    MessageEnvelope env{};  // kind = Unknown
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::InvalidArgument),
                            static_cast<std::uint8_t>(bridge.publish(env).code));
}

static void test_bridge_counts_rx_errors() {
    FakeNetworkTransport transport(kTransportId);
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    const BinaryMessageCodec codec;
    Bridge bridge(session, transport, codec, codec, metrics);
    bringOnline(session, transport);
    TEST_ASSERT_TRUE(bridge.begin().ok());

    // Gültigen Frame erzeugen, dann CRC zerstören und einspeisen.
    std::uint8_t frame[mea::protocol::kMaxFrameSize] = {};
    std::size_t written = 0;
    const MessageEnvelope env =
        mea::protocol::makeMeasurementEnvelope(610, 0, 1, 4242, sampleMeasurement());
    TEST_ASSERT_TRUE(codec.encode(env, frame, sizeof(frame), written).ok());
    frame[written - 1] ^= 0xFFU;  // CRC-Byte kippen
    transport.feedInput(frame, written);

    TEST_ASSERT_TRUE(bridge.update(0).ok());
    MessageEnvelope in{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::NoData),
                            static_cast<std::uint8_t>(bridge.poll(in).code));
    TEST_ASSERT_EQUAL_UINT32(1, metrics.rxErrorCount);
}

// ------------------------------------------------------- NetworkMeasurementSink

static void test_measurement_sink_end_to_end() {
    FakeNetworkTransport transport(kTransportId);
    transport.loopback = true;
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 801);
    const BinaryMessageCodec codec;
    Bridge bridge(session, transport, codec, codec, metrics);
    NetworkMeasurementSink sink(bridge, kBridgeSinkId, 0);

    bringOnline(session, transport);
    TEST_ASSERT_TRUE(bridge.begin().ok());
    TEST_ASSERT_TRUE(sink.begin().ok());
    TEST_ASSERT_EQUAL_UINT16(kBridgeSinkId, sink.id());

    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());
    TEST_ASSERT_TRUE(sink.update(0).ok());  // pumpt Session + I/O

    MessageEnvelope in{};
    TEST_ASSERT_TRUE(bridge.poll(in).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MessageKind::Measurement),
                            static_cast<std::uint8_t>(in.header.kind));
    TEST_ASSERT_EQUAL_UINT16(610, in.header.componentId);
    TEST_ASSERT_EQUAL_FLOAT(3.3F, in.payload.measurement.value);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_backoff_grows_and_caps);
    RUN_TEST(test_session_connects_to_online);
    RUN_TEST(test_session_failed_connect_backs_off_and_retries);
    RUN_TEST(test_session_faults_after_max_attempts);
    RUN_TEST(test_session_connect_timeout);
    RUN_TEST(test_session_reconnects_after_link_loss);
    RUN_TEST(test_session_disconnect_stops_reconnect);
    RUN_TEST(test_bridge_loopback_roundtrip);
    RUN_TEST(test_bridge_full_tx_drops_and_reports_would_block);
    RUN_TEST(test_bridge_rejects_unknown_kind);
    RUN_TEST(test_bridge_counts_rx_errors);
    RUN_TEST(test_measurement_sink_end_to_end);
    return UNITY_END();
}
