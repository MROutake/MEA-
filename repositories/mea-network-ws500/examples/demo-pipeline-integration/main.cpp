/// @file examples/demo-pipeline-integration/main.cpp
/// @brief Referenz-Verdrahtung: die bestehende 74HC595-Pipeline aus
///        main_pipeline_595_nano33ble.cpp wird um einen WS500-Telemetrie-Sink
///        erweitert. Der Netzwerk-Sink ist ein gewöhnlicher IMeasurementSink –
///        der Orchestratorfluss bleibt unverändert (AcquireSensors →
///        HandleCommands → RunPipeline → ApplyOutputs → Publish).
///
/// Diese Datei ist ein DOKUMENTATIONS-Beispiel und wird nicht von der CI
/// gebaut (sie benötigt eine WS500/W5500-Ethernet-Library nach Wahl).
///
/// Benötigte lib_deps (Beispiel):
///   mea-core, mea-processing, mea-managers, mea-state-machine,
///   mea-device-74hc595, mea-protocol, mea-network-core, mea-network-ws500,
///   sowie eine Ethernet-Library, die `EthernetClient` bereitstellt.

#include <Arduino.h>
#include <Ethernet.h>  // WS500/W5500-Treiber der Wahl (liefert EthernetClient)

#include <MeaCore.h>
#include <MeaManagers.h>
#include <MeaNetworkCore.h>
#include <MeaNetworkWs500.h>
#include <MeaProtocol.h>
#include <MeaStateMachine.h>
#include <mea-device-74hc595.h>

namespace {

using namespace mea;

constexpr ComponentId kSourceId = 610;
constexpr ComponentId kRunlightSinkId = 620;
constexpr ComponentId kNetworkSinkId = 621;  // neuer Telemetrie-Sink
constexpr ComponentId kPipelineId = 630;
constexpr ComponentId kWs500Id = 640;
constexpr ComponentId kSessionId = 641;

constexpr TimestampMs kSourceIntervalMs = 120;
constexpr TimestampMs kPipelineCycleIntervalMs = 20;

// --- Messquelle (unverändert aus der bestehenden Demo) -----------------------
class TickSource final : public IMeasurementSource {
public:
    ComponentId id() const noexcept override { return kSourceId; }
    Status begin() noexcept override {
        pending_ = false;
        first_ = true;
        sequence_ = 0;
        lastTickMs_ = 0;
        initialized_ = true;
        return okStatus();
    }
    Status update(TimestampMs nowMs) noexcept override {
        if (!initialized_) return makeStatus(StatusCode::NotInitialized, kSourceId);
        if (pending_) return okStatus();
        if (first_ || intervalElapsed(nowMs, lastTickMs_, kSourceIntervalMs)) {
            first_ = false;
            lastTickMs_ = nowMs;
            pendingMeasurement_ = {};
            pendingMeasurement_.sourceId = kSourceId;
            pendingMeasurement_.value = static_cast<float>(sequence_);
            pendingMeasurement_.sampledAtMs = nowMs;
            pendingMeasurement_.sequence = sequence_;
            ++sequence_;
            pending_ = true;
        }
        return okStatus();
    }
    size_t available() const noexcept override { return pending_ ? 1U : 0U; }
    Status read(Measurement& out) noexcept override {
        if (!pending_) return makeStatus(StatusCode::NoData, kSourceId);
        out = pendingMeasurement_;
        pending_ = false;
        return okStatus();
    }

private:
    bool initialized_{false};
    bool first_{true};
    bool pending_{false};
    TimestampMs lastTickMs_{0};
    SequenceNumber sequence_{0};
    Measurement pendingMeasurement_{};
};

// --- 74HC595-Lauflicht-Sink (unverändert) ------------------------------------
class RunlightSink final : public IMeasurementSink {
public:
    explicit RunlightSink(HC595OutputDriver<2>& driver) noexcept : driver_(driver) {}
    ComponentId id() const noexcept override { return kRunlightSinkId; }
    Status begin() noexcept override {
        const Status status = driver_.begin();
        initialized_ = status.ok();
        hasActivePos_ = false;
        hasPendingPos_ = false;
        return status;
    }
    Status update(TimestampMs) noexcept override {
        if (!initialized_) return makeStatus(StatusCode::NotInitialized, kRunlightSinkId);
        if (!hasPendingPos_) return okStatus();
        if (hasActivePos_) {
            const Status s = driver_.setChannel(currentPos_, false);
            if (!s.ok()) return s;
        }
        Status s = driver_.setChannel(pendingPos_, true);
        if (!s.ok()) return s;
        s = driver_.commit();
        if (!s.ok()) return s;
        currentPos_ = pendingPos_;
        hasActivePos_ = true;
        hasPendingPos_ = false;
        return okStatus();
    }
    size_t capacityAvailable() const noexcept override {
        return (initialized_ && !hasPendingPos_) ? 1U : 0U;
    }
    Status submit(const Measurement& m) noexcept override {
        if (!initialized_) return makeStatus(StatusCode::NotInitialized, kRunlightSinkId);
        if (hasPendingPos_) return makeStatus(StatusCode::WouldBlock, kRunlightSinkId);
        const size_t channels = driver_.channelCount();
        if (channels == 0) return makeStatus(StatusCode::InvalidConfiguration, kRunlightSinkId);
        pendingPos_ = static_cast<size_t>(m.sequence % channels);
        hasPendingPos_ = true;
        return okStatus();
    }

private:
    HC595OutputDriver<2>& driver_;
    bool initialized_{false};
    bool hasActivePos_{false};
    bool hasPendingPos_{false};
    size_t currentPos_{0};
    size_t pendingPos_{0};
};

// --- 74HC595-Stack (unverändert) ---------------------------------------------
ArduinoShiftWriterConfig writerConfig{A3, A0, A1, A2, A4, 0};
Arduino74hc595 writer(writerConfig);
HC595Config driverConfig{kRunlightSinkId, true, false};
HC595OutputDriver<2> outputDriver(writer, driverConfig);

TickSource source;
RunlightSink runlightSink(outputDriver);

// --- WS500-Netzwerkzweig (NEU) -----------------------------------------------
EthernetClient ethClient;
network::ws500::ArduinoWs500Client<EthernetClient> ws500Client(ethClient);
network::ws500::Ws500Config ws500Config{kWs500Id, {10, 0, 0, 2}, 6000, 0, 5000};
network::ws500::Ws500Transport ws500Transport(ws500Client, ws500Config);
network::NetworkMetrics metrics;
network::ReconnectPolicy reconnectPolicy;
network::NetworkSession session(ws500Transport, reconnectPolicy, metrics, kSessionId);
protocol::BinaryMessageCodec codec;
network::ProtocolBridge<512, 512, 4> bridge(session, ws500Transport, codec, codec, metrics);
network::NetworkMeasurementSink networkSink(bridge, kNetworkSinkId, 0);

// Zwei Sinks: Lauflicht + Netzwerk-Telemetrie.
constexpr ComponentId kSinkIds[] = {kRunlightSinkId, kNetworkSinkId};

SensorManager<1> sources;
ProcessorManager<1> processors;
SinkManager<2> sinks;  // Kapazität 2 statt 1: zweiter Sink für Telemetrie
MeasurementPipelineMachine pipeline(
    sources, processors, sinks,
    makePipelineConfig(kPipelineId, kSourceId,
                       ArrayView<const ComponentId>(nullptr, 0),
                       ArrayView<const ComponentId>(kSinkIds, 2),
                       kPipelineCycleIntervalMs, 1000, 1000, {50, 2}, true));

bool ready = false;

}  // namespace

void setup() {
    Serial.begin(115200);
    // Ethernet/WS500 initialisieren (Beispiel; DHCP oder statische IP nach Bedarf).
    // Ethernet.begin(mac);

    sources.registerComponent(source);
    sinks.registerComponent(runlightSink);
    sinks.registerComponent(networkSink);

    session.begin(millis());
    bridge.begin();
    session.connect(millis());

    sources.beginAll();
    processors.beginAll();
    sinks.beginAll();
    if (!pipeline.begin(millis()).ok()) return;
    ready = true;
}

void loop() {
    if (!ready) return;
    const TimestampMs now = millis();
    // Feste Phasenfolge (completion-driven): der Netzwerk-Sink wird über
    // sinks.updateAll() im ApplyOutputs-Schritt getrieben.
    (void)sources.updateAll(now);   // AcquireSensors
    (void)sinks.updateAll(now);     // ApplyOutputs (inkl. WS500-I/O über den Sink)
    (void)pipeline.update(now);     // RunPipeline (Submit an beide Sinks)
}
