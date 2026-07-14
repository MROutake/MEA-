#include <unity.h>

#include <cstdint>
#include <limits>

#include <MeaAnalogInput.h>
#include <mea/device/testing/FakeAnalogReader.h>
#include <mea/testing/ContractChecks.h>

void setUp() {}
void tearDown() {}

namespace {

mea::AnalogInputSensor::Config validConfig() {
    mea::AnalogInputSensor::Config config{};
    config.sourceId = 100;
    config.pin = 34;
    config.sampleIntervalMs = 100;
    config.samplesPerMeasurement = 4;
    config.maxSamplesPerUpdate = 2;
    config.outputKind = mea::MeasurementKind::RawAnalog;
    config.outputUnit = mea::Unit::RawCount;
    return config;
}

/// Führt update() so oft aus, bis ein Messwert vorliegt (höchstens @p maxUpdates).
void updateUntilAvailable(mea::AnalogInputSensor& sensor, mea::TimestampMs& nowMs,
                          const std::uint32_t maxUpdates = 32) {
    for (std::uint32_t index = 0; index < maxUpdates && sensor.available() == 0;
         ++index) {
        TEST_ASSERT_TRUE(sensor.update(nowMs).ok());
        ++nowMs;
    }
}

}  // namespace

// ---------------------------------------------------------------- Konfiguration

static void test_contract_pre_begin() {
    mea::testing::FakeAnalogReader reader;
    mea::AnalogInputSensor sensor(reader, validConfig());
    TEST_ASSERT_NULL(mea::testing::checkSourcePreBeginContract(sensor));
}

static void test_invalid_id_is_rejected() {
    mea::testing::FakeAnalogReader reader;
    auto config = validConfig();
    config.sourceId = mea::InvalidComponentId;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidConfiguration),
        static_cast<std::uint8_t>(sensor.begin().code));
}

static void test_invalid_interval_is_rejected() {
    mea::testing::FakeAnalogReader reader;
    auto config = validConfig();
    config.sampleIntervalMs = 0;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidConfiguration),
        static_cast<std::uint8_t>(sensor.begin().code));
}

static void test_zero_samples_are_rejected() {
    mea::testing::FakeAnalogReader reader;
    auto config = validConfig();
    config.samplesPerMeasurement = 0;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidConfiguration),
        static_cast<std::uint8_t>(sensor.begin().code));
}

static void test_hal_begin_failure_is_propagated_with_origin() {
    mea::testing::FakeAnalogReader reader;
    reader.beginPinResult =
        mea::makeStatus(mea::StatusCode::IoError, mea::InvalidComponentId, 3);
    mea::AnalogInputSensor sensor(reader, validConfig());
    const mea::Status status = sensor.begin();
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::IoError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(100, status.origin);  // Sensor trägt seine ID nach
    TEST_ASSERT_EQUAL_UINT16(3, status.detail);

    // Nicht initialisiert: update() muss NotInitialized melden.
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::NotInitialized),
                            static_cast<std::uint8_t>(sensor.update(0).code));
}

// ---------------------------------------------------------------- Oversampling

static void test_oversampling_averages_and_is_bounded_per_update() {
    mea::testing::FakeAnalogReader reader;
    const std::uint32_t values[] = {100, 200, 300, 400};
    reader.setValues(values, 4);

    mea::AnalogInputSensor sensor(reader, validConfig());  // 4 Samples, max 2/Update
    TEST_ASSERT_TRUE(sensor.begin().ok());

    // 1. Update: startet Messung, nimmt höchstens 2 Samples.
    TEST_ASSERT_TRUE(sensor.update(0).ok());
    TEST_ASSERT_EQUAL_UINT32(2, reader.readCalls);
    TEST_ASSERT_EQUAL_size_t(0, sensor.available());

    // 2. Update: restliche 2 Samples, Messwert fertig.
    TEST_ASSERT_TRUE(sensor.update(1).ok());
    TEST_ASSERT_EQUAL_UINT32(4, reader.readCalls);
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 250.0F, measurement.value);  // (100+200+300+400)/4
    TEST_ASSERT_EQUAL_UINT32(1, measurement.sampledAtMs);          // Abschlusszeitpunkt
    TEST_ASSERT_EQUAL_UINT32(1, measurement.sequence);
    TEST_ASSERT_EQUAL_UINT16(100, measurement.sourceId);
}

static void test_read_failure_aborts_acquisition() {
    mea::testing::FakeAnalogReader reader;
    reader.failAfterReads = 3;  // 4. readRaw schlägt fehl
    mea::AnalogInputSensor sensor(reader, validConfig());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());      // 2 Samples ok
    const mea::Status status = sensor.update(1);  // 3. ok, 4. schlägt fehl
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::IoError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(100, status.origin);
    TEST_ASSERT_EQUAL_size_t(0, sensor.available());
    TEST_ASSERT_EQUAL_UINT32(1, sensor.failedAcquisitions());

    // Nach dem Intervall startet eine neue Messung (Fehler ist nicht dauerhaft).
    reader.failAfterReads = 0;
    mea::TimestampMs now = 200;
    updateUntilAvailable(sensor, now);
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());
}

static void test_sum_overflow_is_detected() {
    mea::testing::FakeAnalogReader reader;
    reader.maximumRaw = std::numeric_limits<std::uint32_t>::max();
    reader.setConstantValue(std::numeric_limits<std::uint32_t>::max());

    auto config = validConfig();
    config.samplesPerMeasurement = 2;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_TRUE(sensor.begin().ok());

    const mea::Status status = sensor.update(0);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::ProcessingError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(1, status.detail);  // Detail 1 = Summenüberlauf
    TEST_ASSERT_EQUAL_UINT32(1, sensor.failedAcquisitions());
}

// ---------------------------------------------------------------- Zeitverhalten

static void test_interval_is_respected() {
    mea::testing::FakeAnalogReader reader;
    reader.setConstantValue(10);
    auto config = validConfig();
    config.samplesPerMeasurement = 1;
    config.maxSamplesPerUpdate = 1;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());  // erste Messung sofort
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    TEST_ASSERT_TRUE(sensor.update(50).ok());  // Intervall (100 ms) nicht erreicht
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    TEST_ASSERT_TRUE(sensor.update(100).ok());  // Intervall erreicht
    TEST_ASSERT_EQUAL_size_t(2, sensor.available());
}

static void test_timestamp_rollover_is_handled() {
    mea::testing::FakeAnalogReader reader;
    reader.setConstantValue(10);
    auto config = validConfig();
    config.samplesPerMeasurement = 1;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_TRUE(sensor.begin().ok());

    const mea::TimestampMs nearMax = std::numeric_limits<std::uint32_t>::max() - 20U;
    TEST_ASSERT_TRUE(sensor.update(nearMax).ok());  // Messung vor dem Überlauf
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    // 21 ms später (nach dem Überlauf): Intervall (100 ms) noch nicht erreicht.
    TEST_ASSERT_TRUE(sensor.update(0).ok());
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    // 100 ms nach Messstart (über den Überlauf hinweg): neue Messung.
    TEST_ASSERT_TRUE(sensor.update(nearMax + 100U).ok());
    TEST_ASSERT_EQUAL_size_t(2, sensor.available());
}

// ---------------------------------------------------------------- Puffer

static void test_full_queue_drops_new_measurement_and_counts() {
    mea::testing::FakeAnalogReader reader;
    reader.setConstantValue(10);
    auto config = validConfig();
    config.samplesPerMeasurement = 1;
    config.sampleIntervalMs = 1;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_TRUE(sensor.begin().ok());

    mea::TimestampMs now = 0;
    // Puffer (Kapazität 4) füllen + 2 weitere Messungen erzwingen.
    for (std::uint32_t index = 0; index < 6; ++index) {
        TEST_ASSERT_TRUE(sensor.update(now).ok());
        now += 10;
    }
    TEST_ASSERT_EQUAL_size_t(mea::AnalogInputSensor::kQueueCapacity, sensor.available());
    TEST_ASSERT_EQUAL_UINT32(2, sensor.droppedMeasurements());

    // Sequenznummern zeigen die Lücke: 1..4 im Puffer, 5 und 6 verworfen.
    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT32(1, measurement.sequence);

    // Nach dem Lesen ist wieder Platz; die nächste Messung trägt Sequenz 7.
    TEST_ASSERT_TRUE(sensor.update(now).ok());
    while (sensor.available() > 0) {
        TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    }
    TEST_ASSERT_EQUAL_UINT32(7, measurement.sequence);
}

static void test_repeated_read_returns_no_data() {
    mea::testing::FakeAnalogReader reader;
    reader.setConstantValue(10);
    auto config = validConfig();
    config.samplesPerMeasurement = 1;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_TRUE(sensor.begin().ok());
    TEST_ASSERT_TRUE(sensor.update(0).ok());

    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    const mea::Status second = sensor.read(measurement);  // keine Daten doppelt
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::NoData),
                            static_cast<std::uint8_t>(second.code));
}

static void test_sequence_numbers_are_monotonic() {
    mea::testing::FakeAnalogReader reader;
    reader.setConstantValue(10);
    auto config = validConfig();
    config.samplesPerMeasurement = 1;
    config.sampleIntervalMs = 1;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_TRUE(sensor.begin().ok());

    mea::SequenceNumber last = 0;
    mea::TimestampMs now = 0;
    for (std::uint32_t index = 0; index < 3; ++index) {
        TEST_ASSERT_TRUE(sensor.update(now).ok());
        now += 10;
        mea::Measurement measurement{};
        TEST_ASSERT_TRUE(sensor.read(measurement).ok());
        TEST_ASSERT_EQUAL_UINT32(last + 1, measurement.sequence);
        last = measurement.sequence;
    }
}

static void test_rebegin_clears_queue_but_keeps_sequence() {
    mea::testing::FakeAnalogReader reader;
    reader.setConstantValue(10);
    auto config = validConfig();
    config.samplesPerMeasurement = 1;
    mea::AnalogInputSensor sensor(reader, config);
    TEST_ASSERT_TRUE(sensor.begin().ok());
    TEST_ASSERT_TRUE(sensor.update(0).ok());
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    TEST_ASSERT_TRUE(sensor.begin().ok());  // reinitialisierend (ADR 0004)
    TEST_ASSERT_EQUAL_size_t(0, sensor.available());

    TEST_ASSERT_TRUE(sensor.update(1000).ok());
    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT32(2, measurement.sequence);  // Sequenz läuft weiter
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_contract_pre_begin);
    RUN_TEST(test_invalid_id_is_rejected);
    RUN_TEST(test_invalid_interval_is_rejected);
    RUN_TEST(test_zero_samples_are_rejected);
    RUN_TEST(test_hal_begin_failure_is_propagated_with_origin);
    RUN_TEST(test_oversampling_averages_and_is_bounded_per_update);
    RUN_TEST(test_read_failure_aborts_acquisition);
    RUN_TEST(test_sum_overflow_is_detected);
    RUN_TEST(test_interval_is_respected);
    RUN_TEST(test_timestamp_rollover_is_handled);
    RUN_TEST(test_full_queue_drops_new_measurement_and_counts);
    RUN_TEST(test_repeated_read_returns_no_data);
    RUN_TEST(test_sequence_numbers_are_monotonic);
    RUN_TEST(test_rebegin_clears_queue_but_keeps_sequence);
    return UNITY_END();
}
