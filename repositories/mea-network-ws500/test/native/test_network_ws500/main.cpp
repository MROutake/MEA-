#include <unity.h>

#include <cstddef>
#include <cstdint>

#include <MeaNetworkCore.h>
#include <MeaNetworkWs500.h>
#include <MeaProtocol.h>
#include <mea/network/ws500/testing/FakeWs500Client.h>

void setUp() {}
void tearDown() {}

using namespace mea;
using namespace mea::network;
using namespace mea::network::ws500;
using mea::network::ws500::testing::FakeWs500Client;
using mea::protocol::BinaryMessageCodec;
using mea::protocol::MessageEnvelope;
using mea::protocol::MessageKind;

namespace {

constexpr ComponentId kTransportId = 900;

Ws500Config makeConfig() {
    Ws500Config config{};
    config.id = kTransportId;
    config.host[0] = 192;
    config.host[1] = 168;
    config.host[2] = 0;
    config.host[3] = 50;
    config.port = 5000;
    config.connectTimeoutMs = 100;
    return config;
}

Measurement sampleMeasurement() {
    Measurement m{};
    m.sourceId = 610;
    m.kind = MeasurementKind::Voltage;
    m.unit = Unit::Volt;
    m.value = 2.5F;
    m.sampledAtMs = 1000;
    return m;
}

}  // namespace

// -------------------------------------------------------------- Error-Mapping

static void test_status_mapping() {
    TEST_ASSERT_TRUE(statusFromWs500(Ws500Result::Ok, kTransportId).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::WouldBlock),
                            static_cast<std::uint8_t>(
                                statusFromWs500(Ws500Result::WouldBlock, kTransportId).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::Busy),
                            static_cast<std::uint8_t>(
                                statusFromWs500(Ws500Result::NotReady, kTransportId).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::Timeout),
                            static_cast<std::uint8_t>(
                                statusFromWs500(Ws500Result::Timeout, kTransportId).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::IoError),
                            static_cast<std::uint8_t>(
                                statusFromWs500(Ws500Result::HardwareFault, kTransportId).code));
    TEST_ASSERT_EQUAL_UINT16(kTransportId,
                             statusFromWs500(Ws500Result::Timeout, kTransportId).origin);
}

// -------------------------------------------------------------- Ws500Transport

static void test_transport_connects_up() {
    FakeWs500Client client;
    Ws500Transport transport(client, makeConfig());
    TEST_ASSERT_TRUE(transport.begin().ok());
    TEST_ASSERT_EQUAL_UINT16(kTransportId, transport.id());

    TEST_ASSERT_TRUE(transport.connect(0).ok());
    TEST_ASSERT_EQUAL(LinkState::Connecting, transport.linkState());
    transport.update(0);
    TEST_ASSERT_EQUAL(LinkState::Up, transport.linkState());
    TEST_ASSERT_GREATER_THAN_size_t(0, transport.writable());
    TEST_ASSERT_EQUAL_UINT32(1, client.connectCalls);
}

static void test_transport_connect_timeout() {
    FakeWs500Client client;
    client.connectImmediately = false;  // Handshake gelingt nie
    Ws500Transport transport(client, makeConfig());
    TEST_ASSERT_TRUE(transport.begin().ok());
    TEST_ASSERT_TRUE(transport.connect(0).ok());
    transport.update(50);
    TEST_ASSERT_EQUAL(LinkState::Connecting, transport.linkState());
    transport.update(100);  // connectTimeoutMs = 100
    TEST_ASSERT_EQUAL(LinkState::Error, transport.linkState());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::Timeout),
                            static_cast<std::uint8_t>(transport.lastStatus().code));
}

static void test_transport_write_read_loopback() {
    FakeWs500Client client;
    client.loopback = true;
    Ws500Transport transport(client, makeConfig());
    TEST_ASSERT_TRUE(transport.begin().ok());
    transport.connect(0);
    transport.update(0);
    TEST_ASSERT_EQUAL(LinkState::Up, transport.linkState());

    const std::uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    std::size_t written = 0;
    TEST_ASSERT_TRUE(transport.write(payload, sizeof(payload), written).ok());
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), written);

    TEST_ASSERT_EQUAL_size_t(sizeof(payload), transport.readable());
    std::uint8_t buffer[8] = {};
    std::size_t readCount = 0;
    TEST_ASSERT_TRUE(transport.read(buffer, sizeof(buffer), readCount).ok());
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), readCount);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, buffer, sizeof(payload));
}

static void test_transport_detects_link_loss() {
    FakeWs500Client client;
    Ws500Transport transport(client, makeConfig());
    TEST_ASSERT_TRUE(transport.begin().ok());
    transport.connect(0);
    transport.update(0);
    TEST_ASSERT_EQUAL(LinkState::Up, transport.linkState());

    client.dropConnection();
    transport.update(1);
    TEST_ASSERT_EQUAL(LinkState::Error, transport.linkState());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::IoError),
                            static_cast<std::uint8_t>(transport.lastStatus().code));
}

static void test_transport_hardware_fault() {
    FakeWs500Client client;
    Ws500Transport transport(client, makeConfig());
    TEST_ASSERT_TRUE(transport.begin().ok());
    transport.connect(0);
    transport.update(0);
    client.injectFault();
    TEST_ASSERT_EQUAL(LinkState::Error, transport.linkState());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::IoError),
                            static_cast<std::uint8_t>(transport.update(1).code));
}

static void test_transport_begin_failure() {
    FakeWs500Client client;
    client.failBegin = true;
    Ws500Transport transport(client, makeConfig());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::IoError),
                            static_cast<std::uint8_t>(transport.begin().code));
    // Ohne erfolgreiches begin() ist connect() nicht möglich.
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::NotInitialized),
                            static_cast<std::uint8_t>(transport.connect(0).code));
}

static void test_transport_invalid_config() {
    FakeWs500Client client;
    Ws500Config config = makeConfig();
    config.id = InvalidComponentId;
    Ws500Transport transport(client, config);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(StatusCode::InvalidConfiguration),
                            static_cast<std::uint8_t>(transport.begin().code));
}

// -------------------------------------------------- Integration (voller Stack)

static ReconnectPolicy fastPolicy() {
    ReconnectPolicy policy{};
    policy.initialBackoffMs = 10;
    policy.maxBackoffMs = 100;
    policy.connectTimeoutMs = 100;
    return policy;
}

static void test_full_stack_send_receive() {
    FakeWs500Client client;
    client.loopback = true;
    Ws500Transport transport(client, makeConfig());
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 901);
    const BinaryMessageCodec codec;
    ProtocolBridge<512, 512, 4> bridge(session, transport, codec, codec, metrics);

    TEST_ASSERT_TRUE(session.begin(0).ok());
    TEST_ASSERT_TRUE(session.connect(0).ok());
    session.update(0);
    TEST_ASSERT_TRUE(session.isOnline());
    TEST_ASSERT_TRUE(bridge.begin().ok());

    const MessageEnvelope out =
        mea::protocol::makeMeasurementEnvelope(610, 0, 1, 1000, sampleMeasurement());
    TEST_ASSERT_TRUE(bridge.publish(out).ok());
    TEST_ASSERT_TRUE(bridge.update(0).ok());

    MessageEnvelope in{};
    TEST_ASSERT_TRUE(bridge.poll(in).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MessageKind::Measurement),
                            static_cast<std::uint8_t>(in.header.kind));
    TEST_ASSERT_EQUAL_FLOAT(2.5F, in.payload.measurement.value);
}

static void test_full_stack_reconnects() {
    FakeWs500Client client;
    client.loopback = true;
    Ws500Transport transport(client, makeConfig());
    NetworkMetrics metrics{};
    NetworkSession session(transport, fastPolicy(), metrics, 901);
    const BinaryMessageCodec codec;
    ProtocolBridge<512, 512, 4> bridge(session, transport, codec, codec, metrics);

    TEST_ASSERT_TRUE(session.begin(0).ok());
    TEST_ASSERT_TRUE(session.connect(0).ok());
    session.update(0);
    TEST_ASSERT_TRUE(session.isOnline());
    TEST_ASSERT_TRUE(bridge.begin().ok());

    // Verbindungsverlust -> Session geht in Backoff.
    client.dropConnection();
    bridge.update(1);
    TEST_ASSERT_EQUAL(NetworkSession::State::Backoff, session.state());
    TEST_ASSERT_EQUAL_UINT32(1, metrics.reconnectCount);

    // Nach Backoff neuer, erfolgreicher Verbindungsaufbau.
    bridge.update(11);  // Backoff (10 ms) abgelaufen -> Connecting
    bridge.update(11);  // Connecting -> Up -> Online
    TEST_ASSERT_TRUE(session.isOnline());

    // Nach Reconnect wieder sendefähig.
    const MessageEnvelope out =
        mea::protocol::makeMeasurementEnvelope(610, 0, 2, 1000, sampleMeasurement());
    TEST_ASSERT_TRUE(bridge.publish(out).ok());
    TEST_ASSERT_TRUE(bridge.update(12).ok());
    MessageEnvelope in{};
    TEST_ASSERT_TRUE(bridge.poll(in).ok());
    TEST_ASSERT_EQUAL_FLOAT(2.5F, in.payload.measurement.value);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_status_mapping);
    RUN_TEST(test_transport_connects_up);
    RUN_TEST(test_transport_connect_timeout);
    RUN_TEST(test_transport_write_read_loopback);
    RUN_TEST(test_transport_detects_link_loss);
    RUN_TEST(test_transport_hardware_fault);
    RUN_TEST(test_transport_begin_failure);
    RUN_TEST(test_transport_invalid_config);
    RUN_TEST(test_full_stack_send_receive);
    RUN_TEST(test_full_stack_reconnects);
    return UNITY_END();
}
