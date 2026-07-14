#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <MeaAnalogInput.h>
#include <MeaCommunication.h>
#include <MeaCore.h>
#include <MeaManagers.h>
#include <MeaNetworkCore.h>
#include <MeaNetworkWs500.h>
#include <MeaProcessing.h>
#include <MeaProtocol.h>
#include <MeaStateMachine.h>
#include <mea/communication/testing/FakeByteTransport.h>
#include <mea/device/testing/FakeAnalogReader.h>
#include <mea/network/ws500/testing/FakeWs500Client.h>

#include "AppConfig.h"
#include "AppIds.h"

/// @file test_pipeline_network/main.cpp
/// @brief Integrationsbeispiel: die bestehende Demo-Pipeline wird um einen
///        zweiten Sink erweitert, der Messwerte über den WS500-Netzwerk-Stack
///        publiziert. Der bestehende Datenfluss (Serial/CSV) bleibt unverändert;
///        der Netzwerk-Sink fügt sich als gewöhnlicher IMeasurementSink in
///        exakt dieselbe Phasenfolge ein (AcquireSensors → HandleCommands →
///        RunPipeline → ApplyOutputs → Publish).

void setUp() {}
void tearDown() {}

namespace {

using mea::protocol::BinaryMessageCodec;
using mea::protocol::MessageEnvelope;
using mea::protocol::MessageKind;

constexpr mea::ComponentId kNetworkSinkId = 310;
constexpr mea::ComponentId kWs500Id = 320;
constexpr mea::ComponentId kSessionId = 330;

struct TestBench {
    // --- bestehende Demo-Pipeline (unverändert) ---
    mea::testing::FakeAnalogReader analogReader{};
    mea::AnalogInputSensor analogSensor{analogReader,
                                        {
                                            app::ids::AnalogInput1,
                                            app::board::kAnalogInputPin,
                                            app::config::kSensorSampleIntervalMs,
                                            app::config::kSamplesPerMeasurement,
                                            app::config::kMaxSamplesPerUpdate,
                                            mea::MeasurementKind::RawAnalog,
                                            mea::Unit::RawCount,
                                        }};
    mea::LinearProcessor rawToVoltage{{
        app::ids::RawToVoltage,
        app::config::kAdcToVoltGain,
        app::config::kVoltageOffset,
        mea::MeasurementKind::RawAnalog,
        mea::Unit::RawCount,
        mea::MeasurementKind::Voltage,
        mea::Unit::Volt,
    }};
    mea::ClampProcessor voltageClamp{{
        app::ids::VoltageClamp,
        app::config::kVoltageMin,
        app::config::kVoltageMax,
        mea::MeasurementKind::Voltage,
        mea::Unit::Volt,
    }};
    mea::testing::FakeByteTransport serialTransport{};
    mea::CsvMeasurementEncoder csvEncoder{{';', app::config::kCsvDecimalPlaces}};
    mea::BufferedMeasurementSink<app::config::kSinkQueueCapacity,
                                 app::config::kSinkFrameSize>
        serialSink{serialTransport, csvEncoder, app::ids::SerialOutput};

    // --- neuer Netzwerk-Zweig (WS500) ---
    mea::network::ws500::Ws500Config ws500Config{kWs500Id, {10, 0, 0, 2}, 6000, 0, 5000};
    mea::network::ws500::testing::FakeWs500Client ws500Client{};
    mea::network::ws500::Ws500Transport ws500Transport{ws500Client, ws500Config};
    mea::network::NetworkMetrics metrics{};
    mea::network::ReconnectPolicy reconnectPolicy{};
    mea::network::NetworkSession session{ws500Transport, reconnectPolicy, metrics,
                                         kSessionId};
    BinaryMessageCodec codec{};
    mea::network::ProtocolBridge<512, 512, 8> bridge{session, ws500Transport, codec, codec,
                                                     metrics};
    mea::network::NetworkMeasurementSink networkSink{bridge, kNetworkSinkId, 0};

    mea::SensorManager<app::config::kManagerCapacity> sources{};
    mea::ProcessorManager<app::config::kManagerCapacity> processors{};
    mea::SinkManager<app::config::kManagerCapacity> sinks{};

    static constexpr mea::ComponentId kProcessorIds[2] = {app::ids::RawToVoltage,
                                                          app::ids::VoltageClamp};
    // Zwei Sinks: bestehender Serial-Sink + neuer Netzwerk-Sink.
    static constexpr mea::ComponentId kSinkIds[2] = {app::ids::SerialOutput,
                                                     kNetworkSinkId};

    mea::MeasurementPipelineMachine pipeline{
        sources, processors, sinks,
        mea::PipelineConfig{
            app::ids::MeasurementPipeline,
            app::ids::AnalogInput1,
            mea::ArrayView<const mea::ComponentId>(kProcessorIds, 2),
            mea::ArrayView<const mea::ComponentId>(kSinkIds, 2),
            app::config::kPipelineCycleIntervalMs,
            app::config::kAcquisitionTimeoutMs,
            app::config::kPublishTimeoutMs,
            app::config::kRetryPolicy,
            app::config::kStartImmediately,
        }};

    void begin() {
        // Loopback: der Fake spiegelt gesendete Bytes zurück, damit der Test die
        // gesendeten Frames als gültige Nachrichten wieder decodieren kann.
        ws500Client.loopback = true;

        TEST_ASSERT_TRUE(sources.registerComponent(analogSensor).ok());
        TEST_ASSERT_TRUE(processors.registerComponent(rawToVoltage).ok());
        TEST_ASSERT_TRUE(processors.registerComponent(voltageClamp).ok());
        TEST_ASSERT_TRUE(sinks.registerComponent(serialSink).ok());
        TEST_ASSERT_TRUE(sinks.registerComponent(networkSink).ok());

        TEST_ASSERT_TRUE(serialTransport.begin().ok());
        // Netzwerk-Stack initialisieren und Verbindung anfordern.
        TEST_ASSERT_TRUE(session.begin(0).ok());
        TEST_ASSERT_TRUE(bridge.begin().ok());
        TEST_ASSERT_TRUE(session.connect(0).ok());

        TEST_ASSERT_TRUE(sources.beginAll().ok());
        TEST_ASSERT_TRUE(processors.beginAll().ok());
        TEST_ASSERT_TRUE(sinks.beginAll().ok());
        TEST_ASSERT_TRUE(pipeline.begin(0).ok());
    }

    // Exakt die Phasenfolge des Orchestrators (completion-driven):
    // AcquireSensors -> HandleCommands -> RunPipeline -> ApplyOutputs -> Publish.
    void step(const mea::TimestampMs nowMs) {
        (void)sources.updateAll(nowMs);   // AcquireSensors
        (void)serialTransport.update(nowMs);
        (void)sinks.updateAll(nowMs);     // ApplyOutputs (pumpt auch Netzwerk-I/O)
        (void)pipeline.update(nowMs);     // RunPipeline (Submit an beide Sinks)
    }

    void runUntil(const mea::TimestampMs endMs, mea::TimestampMs stepMs = 10) {
        for (mea::TimestampMs now = 0; now <= endMs; now += stepMs) {
            step(now);
        }
    }
};

constexpr mea::ComponentId TestBench::kProcessorIds[2];
constexpr mea::ComponentId TestBench::kSinkIds[2];

std::uint32_t countMeasurements(TestBench& bench) {
    std::uint32_t count = 0;
    MessageEnvelope envelope{};
    while (bench.bridge.poll(envelope).ok()) {
        if (envelope.header.kind == MessageKind::Measurement) {
            ++count;
        }
    }
    return count;
}

}  // namespace

// Beide Sinks erhalten jeden Messwert: die bestehende CSV-Ausgabe bleibt intakt,
// zusätzlich gehen Measurement-Frames über den WS500-Transport.
static void test_network_sink_added_without_breaking_pipeline() {
    TestBench bench;
    bench.analogReader.setConstantValue(2048);
    bench.begin();

    bench.runUntil(3000);

    // Orchestratorfluss unverändert: Zyklen laufen fehlerfrei.
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, bench.pipeline.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(0, bench.pipeline.failedCycles());

    // Bestehender Datenpfad (Serial/CSV) unverändert vorhanden.
    TEST_ASSERT_NOT_NULL(std::strstr(bench.serialTransport.outputText(), "1;100;2;2;1.650;"));

    // Netzwerk-Zweig: Verbindung steht und Frames wurden gesendet/empfangen.
    TEST_ASSERT_TRUE(bench.session.isOnline());
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, bench.metrics.txFrameCount);

    // Empfangene (Loopback-)Frames sind gültige Measurement-Nachrichten.
    const std::uint32_t received = countMeasurements(bench);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, received);
}

// Der Netzwerk-Transport überlebt einen Verbindungsverlust und baut neu auf,
// ohne die Pipeline zu stören (der Serial-Pfad läuft durchgehend weiter).
static void test_pipeline_survives_network_reconnect() {
    TestBench bench;
    bench.analogReader.setConstantValue(1000);
    bench.begin();

    bench.runUntil(2000);
    const std::uint32_t cyclesBefore = bench.pipeline.completedCycles();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, cyclesBefore);

    // Verbindungsverlust simulieren.
    bench.ws500Client.dropConnection();
    for (mea::TimestampMs now = 2010; now <= 2100; now += 10) {
        bench.step(now);
    }
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, bench.metrics.reconnectCount);

    // Weiterlaufen: Session verbindet neu, Pipeline zählt weiter Zyklen.
    bench.ws500Client.connectImmediately = true;
    for (mea::TimestampMs now = 2110; now <= 6000; now += 10) {
        bench.step(now);
    }
    TEST_ASSERT_TRUE(bench.session.isOnline());
    TEST_ASSERT_GREATER_THAN_UINT32(cyclesBefore, bench.pipeline.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(0, bench.pipeline.failedCycles());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_network_sink_added_without_breaking_pipeline);
    RUN_TEST(test_pipeline_survives_network_reconnect);
    return UNITY_END();
}
