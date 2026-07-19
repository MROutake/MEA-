#include <unity.h>

#include <cstdint>

#include <MeaAht20.h>
#include <mea/device/aht20/testing/FakeAht20Driver.h>
#include <mea/testing/ContractChecks.h>

void setUp() {}
void tearDown() {}

namespace {

constexpr mea::ComponentId kTemperatureId = 110;
constexpr mea::ComponentId kHumidityId = 111;

mea::Aht20Device::Config deviceConfig() {
    mea::Aht20Device::Config config{};
    config.sampleIntervalMs = 100;
    config.measurementTimeoutMs = 50;
    return config;
}

mea::Aht20Sensor::Config temperatureConfig() {
    mea::Aht20Sensor::Config config{};
    config.sourceId = kTemperatureId;
    config.channel = mea::Aht20Sensor::Channel::Temperature;
    return config;
}

mea::Aht20Sensor::Config humidityConfig() {
    mea::Aht20Sensor::Config config{};
    config.sourceId = kHumidityId;
    config.channel = mea::Aht20Sensor::Channel::Humidity;
    return config;
}

void assertStatusCode(const mea::StatusCode expected, const mea::Status& status) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(expected),
                            static_cast<std::uint8_t>(status.code));
}

}  // namespace

// ---------------------------------------------------------------- Verträge

static void test_contract_pre_begin() {
    mea::testing::FakeAht20Driver driver;
    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_NULL(mea::testing::checkSourcePreBeginContract(sensor));
}

static void test_invalid_id_is_rejected() {
    mea::testing::FakeAht20Driver driver;
    mea::Aht20Device device(driver, deviceConfig());
    auto config = temperatureConfig();
    config.sourceId = mea::InvalidComponentId;
    mea::Aht20Sensor sensor(device, config);
    TEST_ASSERT_TRUE(device.begin().ok());
    assertStatusCode(mea::StatusCode::InvalidConfiguration, sensor.begin());
}

static void test_sensor_begin_requires_device_begin() {
    mea::testing::FakeAht20Driver driver;
    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor sensor(device, temperatureConfig());
    assertStatusCode(mea::StatusCode::NotInitialized, sensor.begin());
}

static void test_invalid_device_config_is_rejected() {
    mea::testing::FakeAht20Driver driver;
    auto config = deviceConfig();
    config.sampleIntervalMs = 0;
    mea::Aht20Device device(driver, config);
    assertStatusCode(mea::StatusCode::InvalidConfiguration, device.begin());

    auto timeoutConfig = deviceConfig();
    timeoutConfig.measurementTimeoutMs = 0;
    mea::Aht20Device timeoutDevice(driver, timeoutConfig);
    assertStatusCode(mea::StatusCode::InvalidConfiguration, timeoutDevice.begin());
}

static void test_driver_begin_failure_is_propagated() {
    mea::testing::FakeAht20Driver driver;
    driver.beginResult = mea::makeStatus(mea::StatusCode::IoError,
                                         mea::InvalidComponentId, 2);
    mea::Aht20Device device(driver, deviceConfig());
    const mea::Status status = device.begin();
    assertStatusCode(mea::StatusCode::IoError, status);
    TEST_ASSERT_EQUAL_UINT16(2, status.detail);
    assertStatusCode(mea::StatusCode::NotInitialized, device.poll(0));
}

// ---------------------------------------------------------------- Messzyklus

static void test_both_channels_receive_same_sample() {
    mea::testing::FakeAht20Driver driver;
    driver.sample = {21.5F, 55.0F};
    driver.busyPollsPerMeasurement = 1;

    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor temperature(device, temperatureConfig());
    mea::Aht20Sensor humidity(device, humidityConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(temperature.begin().ok());
    TEST_ASSERT_TRUE(humidity.begin().ok());

    // Tick 0: Trigger; Tick 1: Busy; Tick 2: Sample fertig.
    for (mea::TimestampMs now = 0; now <= 2; ++now) {
        TEST_ASSERT_TRUE(temperature.update(now).ok());
        TEST_ASSERT_TRUE(humidity.update(now).ok());
    }
    TEST_ASSERT_EQUAL_size_t(1, temperature.available());
    TEST_ASSERT_EQUAL_size_t(1, humidity.available());

    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(temperature.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT16(kTemperatureId, measurement.sourceId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::MeasurementKind::Temperature),
                            static_cast<std::uint8_t>(measurement.kind));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::Unit::DegreeCelsius),
                            static_cast<std::uint8_t>(measurement.unit));
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 21.5F, measurement.value);
    TEST_ASSERT_EQUAL_UINT32(1, measurement.sequence);

    TEST_ASSERT_TRUE(humidity.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT16(kHumidityId, measurement.sourceId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::MeasurementKind::Humidity),
                            static_cast<std::uint8_t>(measurement.kind));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::Unit::Percent),
                            static_cast<std::uint8_t>(measurement.unit));
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 55.0F, measurement.value);

    // Nur eine Messung wurde ausgelöst (Kanäle teilen sich das Device).
    TEST_ASSERT_EQUAL_UINT32(1, driver.triggerCalls);
}

static void test_interval_is_respected() {
    mea::testing::FakeAht20Driver driver;
    mea::Aht20Device device(driver, deviceConfig());  // Intervall 100 ms
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());  // Trigger
    TEST_ASSERT_TRUE(sensor.update(1).ok());  // Sample fertig
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    TEST_ASSERT_TRUE(sensor.update(50).ok());  // Intervall nicht erreicht
    TEST_ASSERT_EQUAL_UINT32(1, driver.triggerCalls);

    TEST_ASSERT_TRUE(sensor.update(100).ok());  // Intervall erreicht: neuer Trigger
    TEST_ASSERT_TRUE(sensor.update(101).ok());
    TEST_ASSERT_EQUAL_UINT32(2, driver.triggerCalls);
    TEST_ASSERT_EQUAL_size_t(2, sensor.available());
}

static void test_measurement_timeout_aborts_cycle() {
    mea::testing::FakeAht20Driver driver;
    driver.busyPollsPerMeasurement = 1000;  // bleibt dauerhaft Busy
    mea::Aht20Device device(driver, deviceConfig());  // Timeout 50 ms
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());   // Trigger
    TEST_ASSERT_TRUE(sensor.update(10).ok());  // Busy, Timeout läuft
    const mea::Status status = sensor.update(50);
    assertStatusCode(mea::StatusCode::Timeout, status);
    TEST_ASSERT_EQUAL_UINT16(kTemperatureId, status.origin);
    TEST_ASSERT_EQUAL_UINT32(1, device.failedAcquisitions());
    TEST_ASSERT_EQUAL_size_t(0, sensor.available());

    // Nach dem Intervall startet ein neuer Versuch.
    driver.busyPollsPerMeasurement = 0;
    TEST_ASSERT_TRUE(sensor.update(100).ok());  // neuer Trigger
    TEST_ASSERT_TRUE(sensor.update(101).ok());  // Sample fertig
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());
}

static void test_checksum_error_is_counted_and_recovered() {
    mea::testing::FakeAht20Driver driver;
    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    driver.readResult =
        mea::makeStatus(mea::StatusCode::ChecksumError, mea::InvalidComponentId);
    TEST_ASSERT_TRUE(sensor.update(0).ok());  // Trigger
    const mea::Status status = sensor.update(1);
    assertStatusCode(mea::StatusCode::ChecksumError, status);
    TEST_ASSERT_EQUAL_UINT32(1, device.failedAcquisitions());

    driver.readResult = mea::okStatus();
    TEST_ASSERT_TRUE(sensor.update(100).ok());  // neuer Trigger nach Intervall
    TEST_ASSERT_TRUE(sensor.update(101).ok());
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());
}

static void test_trigger_failure_retries_after_interval() {
    mea::testing::FakeAht20Driver driver;
    driver.triggerResult =
        mea::makeStatus(mea::StatusCode::IoError, mea::InvalidComponentId, 4);
    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    const mea::Status status = sensor.update(0);
    assertStatusCode(mea::StatusCode::IoError, status);
    TEST_ASSERT_EQUAL_UINT16(kTemperatureId, status.origin);

    // Kein sofortiger neuer Versuch: erst nach dem Intervall.
    TEST_ASSERT_TRUE(sensor.update(10).ok());
    TEST_ASSERT_EQUAL_UINT32(1, driver.triggerCalls);
    assertStatusCode(mea::StatusCode::IoError, sensor.update(100));
    TEST_ASSERT_EQUAL_UINT32(2, driver.triggerCalls);
}

// ---------------------------------------------------------------- Puffer

static void test_full_queue_drops_new_measurement_and_counts() {
    mea::testing::FakeAht20Driver driver;
    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    // Queue (Kapazität 4) füllen + 2 weitere Messungen erzwingen.
    mea::TimestampMs now = 0;
    for (std::uint32_t index = 0; index < 6; ++index) {
        TEST_ASSERT_TRUE(sensor.update(now).ok());      // Trigger
        TEST_ASSERT_TRUE(sensor.update(now + 1).ok());  // Sample fertig
        now += 100;
    }
    TEST_ASSERT_EQUAL_size_t(mea::Aht20Sensor::kQueueCapacity, sensor.available());
    TEST_ASSERT_EQUAL_UINT32(2, sensor.droppedMeasurements());

    // Sequenznummern zeigen die Lücke: 1..4 im Puffer, 5 und 6 verworfen.
    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT32(1, measurement.sequence);
    while (sensor.available() > 0) {
        TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    }
    TEST_ASSERT_EQUAL_UINT32(4, measurement.sequence);

    TEST_ASSERT_TRUE(sensor.update(now).ok());
    TEST_ASSERT_TRUE(sensor.update(now + 1).ok());
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT32(7, measurement.sequence);
}

static void test_repeated_read_returns_no_data() {
    mea::testing::FakeAht20Driver driver;
    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());
    TEST_ASSERT_NULL(mea::testing::checkSourceEmptyReadContract(sensor));
}

static void test_rebegin_clears_queue_but_keeps_sequence() {
    mea::testing::FakeAht20Driver driver;
    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());
    TEST_ASSERT_TRUE(sensor.update(1).ok());
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    TEST_ASSERT_TRUE(sensor.begin().ok());  // reinitialisierend (ADR 0004)
    TEST_ASSERT_EQUAL_size_t(0, sensor.available());

    TEST_ASSERT_TRUE(sensor.update(100).ok());
    TEST_ASSERT_TRUE(sensor.update(101).ok());
    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT32(2, measurement.sequence);  // Sequenz läuft weiter
}

static void test_sampled_at_is_completion_time() {
    mea::testing::FakeAht20Driver driver;
    driver.busyPollsPerMeasurement = 2;
    mea::Aht20Device device(driver, deviceConfig());
    mea::Aht20Sensor sensor(device, temperatureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());   // Trigger
    TEST_ASSERT_TRUE(sensor.update(10).ok());  // Busy
    TEST_ASSERT_TRUE(sensor.update(20).ok());  // Busy
    TEST_ASSERT_TRUE(sensor.update(30).ok());  // fertig

    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT32(30, measurement.sampledAtMs);  // Abschluss (ADR 0003)
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_contract_pre_begin);
    RUN_TEST(test_invalid_id_is_rejected);
    RUN_TEST(test_sensor_begin_requires_device_begin);
    RUN_TEST(test_invalid_device_config_is_rejected);
    RUN_TEST(test_driver_begin_failure_is_propagated);
    RUN_TEST(test_both_channels_receive_same_sample);
    RUN_TEST(test_interval_is_respected);
    RUN_TEST(test_measurement_timeout_aborts_cycle);
    RUN_TEST(test_checksum_error_is_counted_and_recovered);
    RUN_TEST(test_trigger_failure_retries_after_interval);
    RUN_TEST(test_full_queue_drops_new_measurement_and_counts);
    RUN_TEST(test_repeated_read_returns_no_data);
    RUN_TEST(test_rebegin_clears_queue_but_keeps_sequence);
    RUN_TEST(test_sampled_at_is_completion_time);
    return UNITY_END();
}
