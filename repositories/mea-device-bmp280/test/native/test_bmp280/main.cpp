#include <unity.h>

#include <cstdint>

#include <MeaBmp280.h>
#include <mea/device/bmp280/testing/FakeBmp280Driver.h>
#include <mea/testing/ContractChecks.h>

void setUp() {}
void tearDown() {}

namespace {

constexpr mea::ComponentId kTemperatureId = 120;
constexpr mea::ComponentId kPressureId = 121;

mea::Bmp280Device::Config deviceConfig() {
    mea::Bmp280Device::Config config{};
    config.sampleIntervalMs = 100;
    return config;
}

mea::Bmp280Sensor::Config temperatureConfig() {
    mea::Bmp280Sensor::Config config{};
    config.sourceId = kTemperatureId;
    config.channel = mea::Bmp280Sensor::Channel::Temperature;
    return config;
}

mea::Bmp280Sensor::Config pressureConfig() {
    mea::Bmp280Sensor::Config config{};
    config.sourceId = kPressureId;
    config.channel = mea::Bmp280Sensor::Channel::Pressure;
    return config;
}

void assertStatusCode(const mea::StatusCode expected, const mea::Status& status) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(expected),
                            static_cast<std::uint8_t>(status.code));
}

}  // namespace

// ---------------------------------------------------------------- Verträge

static void test_contract_pre_begin() {
    mea::testing::FakeBmp280Driver driver;
    mea::Bmp280Device device(driver, deviceConfig());
    mea::Bmp280Sensor sensor(device, pressureConfig());
    TEST_ASSERT_NULL(mea::testing::checkSourcePreBeginContract(sensor));
}

static void test_invalid_id_is_rejected() {
    mea::testing::FakeBmp280Driver driver;
    mea::Bmp280Device device(driver, deviceConfig());
    auto config = pressureConfig();
    config.sourceId = mea::InvalidComponentId;
    mea::Bmp280Sensor sensor(device, config);
    TEST_ASSERT_TRUE(device.begin().ok());
    assertStatusCode(mea::StatusCode::InvalidConfiguration, sensor.begin());
}

static void test_sensor_begin_requires_device_begin() {
    mea::testing::FakeBmp280Driver driver;
    mea::Bmp280Device device(driver, deviceConfig());
    mea::Bmp280Sensor sensor(device, pressureConfig());
    assertStatusCode(mea::StatusCode::NotInitialized, sensor.begin());
}

static void test_invalid_device_config_is_rejected() {
    mea::testing::FakeBmp280Driver driver;
    auto config = deviceConfig();
    config.sampleIntervalMs = 0;
    mea::Bmp280Device device(driver, config);
    assertStatusCode(mea::StatusCode::InvalidConfiguration, device.begin());
}

static void test_driver_begin_failure_is_propagated() {
    mea::testing::FakeBmp280Driver driver;
    driver.beginResult =
        mea::makeStatus(mea::StatusCode::ProtocolError, mea::InvalidComponentId, 0x60);
    mea::Bmp280Device device(driver, deviceConfig());
    const mea::Status status = device.begin();
    assertStatusCode(mea::StatusCode::ProtocolError, status);
    TEST_ASSERT_EQUAL_UINT16(0x60, status.detail);
    assertStatusCode(mea::StatusCode::NotInitialized, device.poll(0));
}

// ---------------------------------------------------------------- Messzyklus

static void test_both_channels_receive_same_sample() {
    mea::testing::FakeBmp280Driver driver;
    driver.sample = {23.4F, 101325.0F};

    mea::Bmp280Device device(driver, deviceConfig());
    mea::Bmp280Sensor temperature(device, temperatureConfig());
    mea::Bmp280Sensor pressure(device, pressureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(temperature.begin().ok());
    TEST_ASSERT_TRUE(pressure.begin().ok());

    TEST_ASSERT_TRUE(temperature.update(0).ok());
    TEST_ASSERT_TRUE(pressure.update(0).ok());
    TEST_ASSERT_EQUAL_size_t(1, temperature.available());
    TEST_ASSERT_EQUAL_size_t(1, pressure.available());

    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(temperature.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT16(kTemperatureId, measurement.sourceId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::MeasurementKind::Temperature),
                            static_cast<std::uint8_t>(measurement.kind));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::Unit::DegreeCelsius),
                            static_cast<std::uint8_t>(measurement.unit));
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 23.4F, measurement.value);
    TEST_ASSERT_EQUAL_UINT32(1, measurement.sequence);

    TEST_ASSERT_TRUE(pressure.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT16(kPressureId, measurement.sourceId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::MeasurementKind::Pressure),
                            static_cast<std::uint8_t>(measurement.kind));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::Unit::Pascal),
                            static_cast<std::uint8_t>(measurement.unit));
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 101325.0F, measurement.value);

    // Nur eine Lesung wurde ausgelöst (Kanäle teilen sich das Device).
    TEST_ASSERT_EQUAL_UINT32(1, driver.readCalls);
}

static void test_interval_is_respected() {
    mea::testing::FakeBmp280Driver driver;
    mea::Bmp280Device device(driver, deviceConfig());  // Intervall 100 ms
    mea::Bmp280Sensor sensor(device, pressureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    TEST_ASSERT_TRUE(sensor.update(50).ok());  // Intervall nicht erreicht
    TEST_ASSERT_EQUAL_UINT32(1, driver.readCalls);

    TEST_ASSERT_TRUE(sensor.update(100).ok());  // Intervall erreicht
    TEST_ASSERT_EQUAL_UINT32(2, driver.readCalls);
    TEST_ASSERT_EQUAL_size_t(2, sensor.available());
}

static void test_busy_first_conversion_is_retried_immediately() {
    mea::testing::FakeBmp280Driver driver;
    driver.sample = {20.0F, 100000.0F};
    driver.busyReadsRemaining = 2;  // erste Konversion läuft noch

    mea::Bmp280Device device(driver, deviceConfig());
    mea::Bmp280Sensor sensor(device, pressureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());  // Busy: kein Fehler, kein Sample
    TEST_ASSERT_TRUE(sensor.update(1).ok());  // Busy: sofortiger neuer Versuch
    TEST_ASSERT_EQUAL_size_t(0, sensor.available());
    TEST_ASSERT_EQUAL_UINT32(0, device.failedAcquisitions());

    TEST_ASSERT_TRUE(sensor.update(2).ok());  // Konversion fertig
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());
}

static void test_read_failure_is_counted_and_retried_after_interval() {
    mea::testing::FakeBmp280Driver driver;
    driver.readResult = mea::makeStatus(mea::StatusCode::IoError,
                                        mea::InvalidComponentId, 2);
    mea::Bmp280Device device(driver, deviceConfig());
    mea::Bmp280Sensor sensor(device, pressureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    const mea::Status status = sensor.update(0);
    assertStatusCode(mea::StatusCode::IoError, status);
    TEST_ASSERT_EQUAL_UINT16(kPressureId, status.origin);
    TEST_ASSERT_EQUAL_UINT32(1, device.failedAcquisitions());

    // Kein sofortiger neuer Versuch: erst nach dem Intervall.
    TEST_ASSERT_TRUE(sensor.update(10).ok());
    TEST_ASSERT_EQUAL_UINT32(1, driver.readCalls);

    driver.readResult = mea::okStatus();
    TEST_ASSERT_TRUE(sensor.update(100).ok());
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());
}

// ---------------------------------------------------------------- Puffer

static void test_full_queue_drops_new_measurement_and_counts() {
    mea::testing::FakeBmp280Driver driver;
    mea::Bmp280Device device(driver, deviceConfig());
    mea::Bmp280Sensor sensor(device, pressureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    // Queue (Kapazität 4) füllen + 2 weitere Messungen erzwingen.
    mea::TimestampMs now = 0;
    for (std::uint32_t index = 0; index < 6; ++index) {
        TEST_ASSERT_TRUE(sensor.update(now).ok());
        now += 100;
    }
    TEST_ASSERT_EQUAL_size_t(mea::Bmp280Sensor::kQueueCapacity, sensor.available());
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
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT32(7, measurement.sequence);
}

static void test_repeated_read_returns_no_data() {
    mea::testing::FakeBmp280Driver driver;
    mea::Bmp280Device device(driver, deviceConfig());
    mea::Bmp280Sensor sensor(device, pressureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());
    TEST_ASSERT_NULL(mea::testing::checkSourceEmptyReadContract(sensor));
}

static void test_rebegin_clears_queue_but_keeps_sequence() {
    mea::testing::FakeBmp280Driver driver;
    mea::Bmp280Device device(driver, deviceConfig());
    mea::Bmp280Sensor sensor(device, pressureConfig());
    TEST_ASSERT_TRUE(device.begin().ok());
    TEST_ASSERT_TRUE(sensor.begin().ok());

    TEST_ASSERT_TRUE(sensor.update(0).ok());
    TEST_ASSERT_EQUAL_size_t(1, sensor.available());

    TEST_ASSERT_TRUE(sensor.begin().ok());  // reinitialisierend (ADR 0004)
    TEST_ASSERT_EQUAL_size_t(0, sensor.available());

    TEST_ASSERT_TRUE(sensor.update(100).ok());
    mea::Measurement measurement{};
    TEST_ASSERT_TRUE(sensor.read(measurement).ok());
    TEST_ASSERT_EQUAL_UINT32(2, measurement.sequence);  // Sequenz läuft weiter
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
    RUN_TEST(test_busy_first_conversion_is_retried_immediately);
    RUN_TEST(test_read_failure_is_counted_and_retried_after_interval);
    RUN_TEST(test_full_queue_drops_new_measurement_and_counts);
    RUN_TEST(test_repeated_read_returns_no_data);
    RUN_TEST(test_rebegin_clears_queue_but_keeps_sequence);
    return UNITY_END();
}
