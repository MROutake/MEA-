#include <unity.h>

#include <cstdint>
#include <cstring>

#include <MeaAnalogInput.h>
#include <MeaCommunication.h>
#include <MeaCore.h>
#include <MeaManagers.h>
#include <MeaProcessing.h>
#include <MeaStateMachine.h>
#include <mea/communication/testing/FakeByteTransport.h>
#include <mea/device/testing/FakeAnalogReader.h>

#include "AppConfig.h"
#include "AppIds.h"

void setUp() {}
void tearDown() {}

namespace {

/// Nativer End-to-End-Test der kompletten Demo-Pipeline (Abnahmekriterium 13):
/// FakeAnalogReader -> AnalogInputSensor -> LinearProcessor -> ClampProcessor
/// -> BufferedMeasurementSink -> CsvMeasurementEncoder -> FakeByteTransport.
/// Aufbau und Konfiguration entsprechen der Application (AppConfig/AppIds);
/// nur die Hardware-Ränder sind durch Fakes ersetzt.
struct TestBench {
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
    mea::testing::FakeByteTransport transport{};
    mea::CsvMeasurementEncoder encoder{{';', app::config::kCsvDecimalPlaces}};
    mea::BufferedMeasurementSink<app::config::kSinkQueueCapacity,
                                 app::config::kSinkFrameSize>
        serialSink{transport, encoder, app::ids::SerialOutput};

    mea::SensorManager<app::config::kManagerCapacity> sources{};
    mea::ProcessorManager<app::config::kManagerCapacity> processors{};
    mea::SinkManager<app::config::kManagerCapacity> sinks{};

    static constexpr mea::ComponentId kProcessorIds[2] = {app::ids::RawToVoltage,
                                                          app::ids::VoltageClamp};
    static constexpr mea::ComponentId kSinkIds[1] = {app::ids::SerialOutput};

    mea::MeasurementPipelineMachine pipeline{
        sources, processors, sinks,
        mea::PipelineConfig{
            app::ids::MeasurementPipeline,
            app::ids::AnalogInput1,
            mea::ArrayView<const mea::ComponentId>(kProcessorIds, 2),
            mea::ArrayView<const mea::ComponentId>(kSinkIds, 1),
            app::config::kPipelineCycleIntervalMs,
            app::config::kAcquisitionTimeoutMs,
            app::config::kPublishTimeoutMs,
            app::config::kRetryPolicy,
            app::config::kStartImmediately,
        }};

    /// Entspricht Application::begin() (ohne Serial).
    void begin() {
        TEST_ASSERT_TRUE(sources.registerComponent(analogSensor).ok());
        TEST_ASSERT_TRUE(processors.registerComponent(rawToVoltage).ok());
        TEST_ASSERT_TRUE(processors.registerComponent(voltageClamp).ok());
        TEST_ASSERT_TRUE(sinks.registerComponent(serialSink).ok());
        TEST_ASSERT_TRUE(transport.begin().ok());
        TEST_ASSERT_TRUE(sources.beginAll().ok());
        TEST_ASSERT_TRUE(processors.beginAll().ok());
        TEST_ASSERT_TRUE(sinks.beginAll().ok());
        TEST_ASSERT_TRUE(pipeline.begin(0).ok());
    }

    /// Entspricht Application::update().
    void step(const mea::TimestampMs nowMs) {
        (void)sources.updateAll(nowMs);
        (void)transport.update(nowMs);
        (void)sinks.updateAll(nowMs);
        (void)pipeline.update(nowMs);
    }

    void runUntil(const mea::TimestampMs endMs, mea::TimestampMs stepMs = 10) {
        for (mea::TimestampMs now = 0; now <= endMs; now += stepMs) {
            step(now);
        }
    }
};

constexpr mea::ComponentId TestBench::kProcessorIds[2];
constexpr mea::ComponentId TestBench::kSinkIds[1];

}  // namespace

static void test_full_pipeline_produces_csv_line() {
    TestBench bench;
    bench.analogReader.setConstantValue(2048);
    bench.begin();

    bench.runUntil(1500);

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, bench.pipeline.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(0, bench.pipeline.failedCycles());

    // 2048 * (3.3 / 4095) = 1.650... V; CSV: version;src;kind;unit;value;t;seq;q
    const char* output = bench.transport.outputText();
    TEST_ASSERT_NOT_NULL(std::strstr(output, "1;100;2;2;1.650;"));

    // Qualität uneingeschränkt, Wert im Clamp-Bereich.
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 1.650F, bench.pipeline.lastMeasurement().value);
    TEST_ASSERT_EQUAL_UINT16(
        0, static_cast<std::uint16_t>(bench.pipeline.lastMeasurement().quality));
}

static void test_full_pipeline_clamps_out_of_range_values() {
    TestBench bench;
    // Rohwert über dem ADC-Maximum (defekte HAL o. ä.): 8000 * Gain > 3.3 V.
    bench.analogReader.setConstantValue(8000);
    bench.begin();

    bench.runUntil(1500);

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, bench.pipeline.completedCycles());
    const mea::Measurement& last = bench.pipeline.lastMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.001F, app::config::kVoltageMax, last.value);
    TEST_ASSERT_TRUE(mea::hasFlag(last.quality, mea::QualityFlag::OutOfRange));

    // Quality-Feld (Bit 1 = OutOfRange -> 2) steht in der CSV-Zeile.
    TEST_ASSERT_NOT_NULL(std::strstr(bench.transport.outputText(), ";2\n"));
}

static void test_full_pipeline_survives_transport_backpressure() {
    TestBench bench;
    bench.analogReader.setConstantValue(1000);
    bench.begin();

    bench.transport.writableLimit = 0;  // Serial "verstopft"
    bench.runUntil(3000);
    const std::uint32_t stalledCycles = bench.pipeline.completedCycles();

    bench.transport.writableLimit = mea::testing::FakeByteTransport::kBufferSize;
    for (mea::TimestampMs now = 3010; now <= 6000; now += 10) {
        bench.step(now);
    }

    // Nach dem Stau läuft die Pipeline weiter und sendet wieder Frames.
    TEST_ASSERT_GREATER_THAN_UINT32(stalledCycles, bench.pipeline.completedCycles());
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, bench.serialSink.sentFrames());
}

static void test_full_pipeline_multiple_cycles_have_increasing_sequence() {
    TestBench bench;
    bench.analogReader.setConstantValue(100);
    bench.begin();

    bench.runUntil(4500);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(3, bench.pipeline.completedCycles());

    // Mindestens drei CSV-Zeilen mit unterschiedlichen Sequenznummern.
    const char* output = bench.transport.outputText();
    std::uint32_t lines = 0;
    for (const char* cursor = output; *cursor != '\0'; ++cursor) {
        if (*cursor == '\n') {
            ++lines;
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(3, lines);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_full_pipeline_produces_csv_line);
    RUN_TEST(test_full_pipeline_clamps_out_of_range_values);
    RUN_TEST(test_full_pipeline_survives_transport_backpressure);
    RUN_TEST(test_full_pipeline_multiple_cycles_have_increasing_sequence);
    return UNITY_END();
}
