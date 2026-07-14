#include <unity.h>

#include <Arduino.h>

#include <MeaAnalogInput.h>
#include <MeaCommunication.h>
#include <MeaCore.h>
#include <MeaManagers.h>
#include <MeaProcessing.h>
#include <MeaStateMachine.h>

#include "AppConfig.h"
#include "AppIds.h"

void setUp() {}
void tearDown() {}

namespace {

/// Embedded-Smoke-Test: baut die echte Demo-Verdrahtung auf dem ESP32 auf und
/// prüft Registrierung, Initialisierung und einige nicht blockierende Updates.
/// Es wird bewusst keine bestimmte ADC-Spannung erwartet (keine Hardware-Annahme).
mea::ArduinoAnalogReader analogReader(app::board::kAdcMaximumRaw);
mea::AnalogInputSensor analogSensor(analogReader,
                                    {
                                        app::ids::AnalogInput1,
                                        app::board::kAnalogInputPin,
                                        app::config::kSensorSampleIntervalMs,
                                        app::config::kSamplesPerMeasurement,
                                        app::config::kMaxSamplesPerUpdate,
                                        mea::MeasurementKind::RawAnalog,
                                        mea::Unit::RawCount,
                                    });
mea::LinearProcessor rawToVoltage({
    app::ids::RawToVoltage,
    app::config::kAdcToVoltGain,
    app::config::kVoltageOffset,
    mea::MeasurementKind::RawAnalog,
    mea::Unit::RawCount,
    mea::MeasurementKind::Voltage,
    mea::Unit::Volt,
});
mea::ArduinoStreamTransport transport(Serial);
const mea::CsvMeasurementEncoder encoder;
mea::BufferedMeasurementSink<4, 96> sink(transport, encoder, app::ids::SerialOutput);

mea::SensorManager<2> sources;
mea::ProcessorManager<2> processors;
mea::SinkManager<2> sinks;

constexpr mea::ComponentId kProcessorIds[] = {app::ids::RawToVoltage};
constexpr mea::ComponentId kSinkIds[] = {app::ids::SerialOutput};

}  // namespace

static void test_components_register_and_begin() {
    TEST_ASSERT_TRUE(sources.registerComponent(analogSensor).ok());
    TEST_ASSERT_TRUE(processors.registerComponent(rawToVoltage).ok());
    TEST_ASSERT_TRUE(sinks.registerComponent(sink).ok());
    TEST_ASSERT_TRUE(transport.begin().ok());
    TEST_ASSERT_TRUE(sources.beginAll().ok());
    TEST_ASSERT_TRUE(processors.beginAll().ok());
    TEST_ASSERT_TRUE(sinks.beginAll().ok());
}

static void test_pipeline_begins_and_updates_non_blocking() {
    mea::PipelineConfig config{};
    config.pipelineId = app::ids::MeasurementPipeline;
    config.sourceId = app::ids::AnalogInput1;
    config.processorIds = mea::ArrayView<const mea::ComponentId>(kProcessorIds, 1);
    config.sinkIds = mea::ArrayView<const mea::ComponentId>(kSinkIds, 1);
    config.cycleIntervalMs = 100;
    config.acquisitionTimeoutMs = 1000;
    config.publishTimeoutMs = 500;
    config.retry = {100, 1};
    config.startImmediately = true;

    static mea::MeasurementPipelineMachine pipeline(sources, processors, sinks, config);
    TEST_ASSERT_TRUE(pipeline.begin(millis()).ok());

    const std::uint32_t startedAt = millis();
    while (pipeline.completedCycles() == 0 &&
           mea::elapsedMs(millis(), startedAt) < 3000U) {
        const mea::TimestampMs now = millis();
        (void)sources.updateAll(now);
        (void)transport.update(now);
        (void)sinks.updateAll(now);
        (void)pipeline.update(now);
        delay(1);  // nur im Test-Harness, nicht in Library-Code
    }

    TEST_ASSERT_EQUAL_UINT8(0, static_cast<std::uint8_t>(mea::PipelineState::Fault) ==
                                       static_cast<std::uint8_t>(pipeline.state())
                                   ? 1
                                   : 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, pipeline.completedCycles());
}

void setup() {
    delay(2000);  // USB-CDC-Aufzählung abwarten (nur Test-Harness)
    UNITY_BEGIN();
    RUN_TEST(test_components_register_and_begin);
    RUN_TEST(test_pipeline_begins_and_updates_non_blocking);
    UNITY_END();
}

void loop() {}
