#include <unity.h>

#include <cstdint>
#include <cstring>

#include <MeaEspNow.h>
#include <mea/communication/CsvMeasurementEncoder.h>
#include <mea/espnow/testing/FakeEspNowRadio.h>

void setUp() {}
void tearDown() {}

namespace {

using mea::testing::FakeEspNowRadio;

[[nodiscard]] mea::MacAddress mac(const std::uint8_t value) {
    return mea::MacAddress{{value, value, value, value, value, value}};
}

const mea::MacAddress kServerMac = mac(0xAA);
const mea::MacAddress kClientMac = mac(0xBB);

mea::EspNowClient::Config clientConfig() {
    mea::EspNowClient::Config config{};
    config.channelDwellMs = 100;
    config.connectTimeoutMs = 200;
    config.pingIntervalMs = 500;
    config.maximumMissedPongs = 2;
    config.firstChannel = 1;
    return config;
}

mea::EspNowServer::Config serverConfig() {
    mea::EspNowServer::Config config{};
    config.channel = 6;
    config.clientTimeoutMs = 1000;
    return config;
}

void assertStatusCode(const mea::StatusCode expected, const mea::Status& status) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(expected),
                            static_cast<std::uint8_t>(status.code));
}

/// Bringt einen Client in den Zustand Connected (Server auf aktuellem Kanal).
void connectClient(mea::EspNowClient& client, FakeEspNowRadio& radio,
                   mea::TimestampMs& nowMs) {
    TEST_ASSERT_TRUE(client.update(nowMs).ok());  // Discover
    radio.injectControl(mea::EspNowMessageType::Offer, kServerMac);
    ++nowMs;
    TEST_ASSERT_TRUE(client.update(nowMs).ok());  // -> ConnectRequest
    radio.injectControl(mea::EspNowMessageType::ConnectAccept, kServerMac);
    ++nowMs;
    TEST_ASSERT_TRUE(client.update(nowMs).ok());  // -> Connected
    TEST_ASSERT_TRUE(client.connected());
}

class FakeEspNowSession final : public mea::IEspNowSession {
public:
    [[nodiscard]] bool connected() const noexcept override { return connectedValue; }

    mea::Status sendData(const std::uint8_t* payload,
                         const std::size_t size) noexcept override {
        ++sendCalls;
        if (!sendResult.ok()) {
            return sendResult;
        }
        if (size <= mea::kEspNowMaxPayload) {
            std::memcpy(lastPayload, payload, size);
            lastSize = size;
        }
        return mea::okStatus();
    }

    bool connectedValue{false};
    mea::Status sendResult{mea::okStatus()};
    std::uint32_t sendCalls{0};
    std::uint8_t lastPayload[mea::kEspNowMaxPayload]{};
    std::size_t lastSize{0};
};

mea::Measurement sampleMeasurement(const float value) {
    mea::Measurement measurement{};
    measurement.sourceId = 100;
    measurement.kind = mea::MeasurementKind::Temperature;
    measurement.unit = mea::Unit::DegreeCelsius;
    measurement.value = value;
    measurement.sampledAtMs = 12345;
    measurement.sequence = 42;
    return measurement;
}

}  // namespace

// ---------------------------------------------------------------- Client

static void test_client_invalid_config_is_rejected() {
    FakeEspNowRadio radio;
    auto config = clientConfig();
    config.channelDwellMs = 0;
    mea::EspNowClient client(radio, config);
    assertStatusCode(mea::StatusCode::InvalidConfiguration, client.begin());

    auto channelConfig = clientConfig();
    channelConfig.firstChannel = 14;  // > maximumChannel (13)
    mea::EspNowClient channelClient(radio, channelConfig);
    assertStatusCode(mea::StatusCode::InvalidConfiguration, channelClient.begin());

    assertStatusCode(mea::StatusCode::NotInitialized, channelClient.update(0));
}

static void test_client_scans_channels_with_discover_broadcasts() {
    FakeEspNowRadio radio;
    radio.maximumChannelValue = 3;
    mea::EspNowClient client(radio, clientConfig());
    TEST_ASSERT_TRUE(client.begin().ok());

    TEST_ASSERT_TRUE(client.update(0).ok());  // Kanal 1 + Discover
    TEST_ASSERT_EQUAL_UINT8(1, radio.channelValue);
    const auto* discover = radio.lastSentOfType(mea::EspNowMessageType::Discover);
    TEST_ASSERT_NOT_NULL(discover);
    TEST_ASSERT_TRUE(discover->destination == mea::kBroadcastAddress);

    TEST_ASSERT_TRUE(client.update(50).ok());  // Verweildauer läuft noch
    TEST_ASSERT_EQUAL_UINT32(1, radio.sendCalls);

    TEST_ASSERT_TRUE(client.update(100).ok());  // Kanal 2
    TEST_ASSERT_EQUAL_UINT8(2, radio.channelValue);
    TEST_ASSERT_TRUE(client.update(200).ok());  // Kanal 3
    TEST_ASSERT_TRUE(client.update(300).ok());  // wieder Kanal 1 (Wrap)
    TEST_ASSERT_EQUAL_UINT8(1, radio.channelValue);
}

static void test_client_handshake_reaches_connected() {
    FakeEspNowRadio radio;
    mea::EspNowClient client(radio, clientConfig());
    TEST_ASSERT_TRUE(client.begin().ok());

    mea::TimestampMs now = 0;
    connectClient(client, radio, now);

    const auto* request = radio.lastSentOfType(mea::EspNowMessageType::ConnectRequest);
    TEST_ASSERT_NOT_NULL(request);
    TEST_ASSERT_TRUE(request->destination == kServerMac);
    TEST_ASSERT_TRUE(client.serverAddress() == kServerMac);
}

static void test_client_connect_timeout_returns_to_scanning() {
    FakeEspNowRadio radio;
    mea::EspNowClient client(radio, clientConfig());
    TEST_ASSERT_TRUE(client.begin().ok());

    TEST_ASSERT_TRUE(client.update(0).ok());
    radio.injectControl(mea::EspNowMessageType::Offer, kServerMac);
    TEST_ASSERT_TRUE(client.update(1).ok());  // Connecting (Start bei t=1)
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::EspNowClient::State::Connecting),
        static_cast<std::uint8_t>(client.state()));

    TEST_ASSERT_TRUE(client.update(201).ok());  // Timeout (200 ms)
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::EspNowClient::State::Scanning),
        static_cast<std::uint8_t>(client.state()));

    // Suche beginnt auf dem zuletzt genutzten Kanal (Server steht dort meist noch).
    const std::uint8_t channelBefore = radio.channelValue;
    TEST_ASSERT_TRUE(client.update(202).ok());
    TEST_ASSERT_EQUAL_UINT8(channelBefore, radio.channelValue);
}

static void test_client_missed_pongs_trigger_reconnect() {
    FakeEspNowRadio radio;
    mea::EspNowClient client(radio, clientConfig());  // Ping 500 ms, 2 Pongs
    TEST_ASSERT_TRUE(client.begin().ok());

    mea::TimestampMs now = 0;
    connectClient(client, radio, now);  // Connected bei t=2

    TEST_ASSERT_TRUE(client.update(502).ok());  // Ping 1
    TEST_ASSERT_NOT_NULL(radio.lastSentOfType(mea::EspNowMessageType::Ping));
    radio.injectControl(mea::EspNowMessageType::Pong, kServerMac);
    TEST_ASSERT_TRUE(client.update(503).ok());  // Pong: Verbindung frisch

    TEST_ASSERT_TRUE(client.update(1003).ok());  // Ping 2 (keine Antwort mehr)
    TEST_ASSERT_TRUE(client.update(1503).ok());  // Ping 3 (keine Antwort)
    TEST_ASSERT_TRUE(client.connected());

    TEST_ASSERT_TRUE(client.update(2003).ok());  // 3 * 500 ms ohne Pong
    TEST_ASSERT_FALSE(client.connected());
    TEST_ASSERT_EQUAL_UINT32(1, client.reconnects());
}

static void test_client_send_data_requires_connection() {
    FakeEspNowRadio radio;
    mea::EspNowClient client(radio, clientConfig());
    TEST_ASSERT_TRUE(client.begin().ok());

    const std::uint8_t payload[] = {'a', 'b', 'c'};
    assertStatusCode(mea::StatusCode::WouldBlock,
                     client.sendData(payload, sizeof(payload)));

    mea::TimestampMs now = 0;
    connectClient(client, radio, now);
    TEST_ASSERT_TRUE(client.sendData(payload, sizeof(payload)).ok());
    TEST_ASSERT_EQUAL_UINT32(1, client.sentDataFrames());

    const auto* frame = radio.lastSentOfType(mea::EspNowMessageType::Data);
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_TRUE(frame->destination == kServerMac);
    TEST_ASSERT_EQUAL_UINT8(mea::kEspNowHeaderSize + sizeof(payload), frame->length);
    TEST_ASSERT_EQUAL_MEMORY(payload, frame->data + mea::kEspNowHeaderSize,
                             sizeof(payload));
}

static void test_client_counts_protocol_errors() {
    FakeEspNowRadio radio;
    mea::EspNowClient client(radio, clientConfig());
    TEST_ASSERT_TRUE(client.begin().ok());

    radio.injectGarbage(kServerMac);
    TEST_ASSERT_TRUE(client.update(0).ok());
    TEST_ASSERT_EQUAL_UINT32(1, client.protocolErrors());
}

// ---------------------------------------------------------------- Server

static void test_server_begin_sets_channel() {
    FakeEspNowRadio radio;
    mea::EspNowServer server(radio, serverConfig());
    TEST_ASSERT_TRUE(server.begin().ok());
    TEST_ASSERT_EQUAL_UINT8(6, radio.channelValue);

    auto invalid = serverConfig();
    invalid.channel = 0;
    mea::EspNowServer invalidServer(radio, invalid);
    assertStatusCode(mea::StatusCode::InvalidConfiguration, invalidServer.begin());
}

static void test_server_answers_discover_with_offer() {
    FakeEspNowRadio radio;
    mea::EspNowServer server(radio, serverConfig());
    TEST_ASSERT_TRUE(server.begin().ok());

    radio.injectControl(mea::EspNowMessageType::Discover, kClientMac);
    TEST_ASSERT_TRUE(server.update(0).ok());

    const auto* offer = radio.lastSentOfType(mea::EspNowMessageType::Offer);
    TEST_ASSERT_NOT_NULL(offer);
    TEST_ASSERT_TRUE(offer->destination == kClientMac);
    TEST_ASSERT_EQUAL_size_t(0, server.clientCount());  // noch keine Verbindung
}

static void test_server_accepts_connection_and_answers_ping() {
    FakeEspNowRadio radio;
    mea::EspNowServer server(radio, serverConfig());
    TEST_ASSERT_TRUE(server.begin().ok());

    radio.injectControl(mea::EspNowMessageType::ConnectRequest, kClientMac);
    TEST_ASSERT_TRUE(server.update(0).ok());
    TEST_ASSERT_NOT_NULL(radio.lastSentOfType(mea::EspNowMessageType::ConnectAccept));
    TEST_ASSERT_EQUAL_size_t(1, server.clientCount());

    radio.injectControl(mea::EspNowMessageType::Ping, kClientMac);
    TEST_ASSERT_TRUE(server.update(10).ok());
    TEST_ASSERT_NOT_NULL(radio.lastSentOfType(mea::EspNowMessageType::Pong));
}

static void test_server_stores_data_and_readds_unknown_clients() {
    FakeEspNowRadio radio;
    mea::EspNowServer server(radio, serverConfig());
    TEST_ASSERT_TRUE(server.begin().ok());

    // Data von unbekanntem Client (z. B. nach Server-Neustart): implizit aufnehmen.
    const std::uint8_t payload[] = {'1', ';', '2', '\n'};
    radio.injectData(kClientMac, payload, sizeof(payload));
    TEST_ASSERT_TRUE(server.update(0).ok());
    TEST_ASSERT_EQUAL_size_t(1, server.clientCount());
    TEST_ASSERT_EQUAL_size_t(1, server.available());

    mea::EspNowDataFrame frame{};
    TEST_ASSERT_TRUE(server.read(frame).ok());
    TEST_ASSERT_TRUE(frame.source == kClientMac);
    TEST_ASSERT_EQUAL_UINT8(sizeof(payload), frame.length);
    TEST_ASSERT_EQUAL_MEMORY(payload, frame.payload, sizeof(payload));

    assertStatusCode(mea::StatusCode::NoData, server.read(frame));
}

static void test_server_expires_stale_clients() {
    FakeEspNowRadio radio;
    mea::EspNowServer server(radio, serverConfig());  // Timeout 1000 ms
    TEST_ASSERT_TRUE(server.begin().ok());

    radio.injectControl(mea::EspNowMessageType::ConnectRequest, kClientMac);
    TEST_ASSERT_TRUE(server.update(0).ok());
    TEST_ASSERT_EQUAL_size_t(1, server.clientCount());

    TEST_ASSERT_TRUE(server.update(999).ok());
    TEST_ASSERT_EQUAL_size_t(1, server.clientCount());

    TEST_ASSERT_TRUE(server.update(1000).ok());
    TEST_ASSERT_EQUAL_size_t(0, server.clientCount());
    TEST_ASSERT_EQUAL_UINT32(1, server.expiredClients());
    TEST_ASSERT_EQUAL_UINT32(1, radio.removePeerCalls);
    TEST_ASSERT_TRUE(radio.lastRemovedPeer == kClientMac);
}

static void test_server_drops_data_when_queue_full() {
    FakeEspNowRadio radio;
    mea::EspNowServer server(radio, serverConfig());
    TEST_ASSERT_TRUE(server.begin().ok());

    const std::uint8_t payload[] = {'x'};
    for (std::uint32_t index = 0;
         index < mea::EspNowServer::kDataQueueCapacity + 2U; ++index) {
        radio.injectData(kClientMac, payload, sizeof(payload));
    }
    TEST_ASSERT_TRUE(server.update(0).ok());
    TEST_ASSERT_EQUAL_size_t(mea::EspNowServer::kDataQueueCapacity,
                             server.available());
    TEST_ASSERT_EQUAL_UINT32(2, server.droppedFrames());
}

// ---------------------------------------------------------------- Sink

static void test_sink_waits_for_connection_then_sends_bounded() {
    FakeEspNowSession session;
    const mea::CsvMeasurementEncoder encoder;
    mea::EspNowMeasurementSink<4> sink(session, encoder, 301);
    TEST_ASSERT_TRUE(sink.begin().ok());

    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement(1.0F)).ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement(2.0F)).ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement(3.0F)).ok());

    TEST_ASSERT_TRUE(sink.update(0).ok());  // nicht verbunden: nichts senden
    TEST_ASSERT_EQUAL_UINT32(0, session.sendCalls);

    session.connectedValue = true;
    TEST_ASSERT_TRUE(sink.update(1).ok());  // begrenzte Arbeit: 2 je update()
    TEST_ASSERT_EQUAL_UINT32(2, session.sendCalls);
    TEST_ASSERT_TRUE(sink.update(2).ok());
    TEST_ASSERT_EQUAL_UINT32(3, session.sendCalls);
    TEST_ASSERT_EQUAL_UINT32(3, sink.sentFrames());
}

static void test_sink_payload_is_encoded_measurement() {
    FakeEspNowSession session;
    session.connectedValue = true;
    const mea::CsvMeasurementEncoder encoder;
    mea::EspNowMeasurementSink<4> sink(session, encoder, 301);
    TEST_ASSERT_TRUE(sink.begin().ok());

    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement(1.25F)).ok());
    TEST_ASSERT_TRUE(sink.update(0).ok());

    // Erwartete CSV-Zeile (Formatversion 1, Standard-Encoder-Konfiguration).
    const char* expected = "1;100;5;7;1.250;12345;42;0\n";
    TEST_ASSERT_EQUAL_size_t(std::strlen(expected), session.lastSize);
    TEST_ASSERT_EQUAL_MEMORY(expected, session.lastPayload, session.lastSize);
}

static void test_sink_drops_oldest_when_queue_full() {
    FakeEspNowSession session;
    const mea::CsvMeasurementEncoder encoder;
    mea::EspNowMeasurementSink<2> sink(session, encoder, 301);
    TEST_ASSERT_TRUE(sink.begin().ok());

    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement(1.0F)).ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement(2.0F)).ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement(3.0F)).ok());  // verdrängt 1.0
    TEST_ASSERT_EQUAL_UINT32(1, sink.droppedMeasurements());

    session.connectedValue = true;
    TEST_ASSERT_TRUE(sink.update(0).ok());
    TEST_ASSERT_EQUAL_UINT32(2, session.sendCalls);

    // Der älteste Messwert (1.0) wurde verworfen: gesendet wurden 2.0 und 3.0.
    const char* expectedLast = "1;100;5;7;3.000;12345;42;0\n";
    TEST_ASSERT_EQUAL_MEMORY(expectedLast, session.lastPayload,
                             std::strlen(expectedLast));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_client_invalid_config_is_rejected);
    RUN_TEST(test_client_scans_channels_with_discover_broadcasts);
    RUN_TEST(test_client_handshake_reaches_connected);
    RUN_TEST(test_client_connect_timeout_returns_to_scanning);
    RUN_TEST(test_client_missed_pongs_trigger_reconnect);
    RUN_TEST(test_client_send_data_requires_connection);
    RUN_TEST(test_client_counts_protocol_errors);
    RUN_TEST(test_server_begin_sets_channel);
    RUN_TEST(test_server_answers_discover_with_offer);
    RUN_TEST(test_server_accepts_connection_and_answers_ping);
    RUN_TEST(test_server_stores_data_and_readds_unknown_clients);
    RUN_TEST(test_server_expires_stale_clients);
    RUN_TEST(test_server_drops_data_when_queue_full);
    RUN_TEST(test_sink_waits_for_connection_then_sends_bounded);
    RUN_TEST(test_sink_payload_is_encoded_measurement);
    RUN_TEST(test_sink_drops_oldest_when_queue_full);
    return UNITY_END();
}
