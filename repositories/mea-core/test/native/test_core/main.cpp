#include <unity.h>

#include <cmath>
#include <cstdint>
#include <limits>

#include <MeaCore.h>

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- Status

static void test_status_default_is_ok() {
    constexpr mea::Status status{};
    TEST_ASSERT_TRUE(status.ok());
    TEST_ASSERT_FALSE(status.transient());
    TEST_ASSERT_EQUAL_UINT16(mea::InvalidComponentId, status.origin);
}

static void test_status_transient_codes() {
    TEST_ASSERT_TRUE(mea::makeStatus(mea::StatusCode::Busy, 1).transient());
    TEST_ASSERT_TRUE(mea::makeStatus(mea::StatusCode::NoData, 1).transient());
    TEST_ASSERT_TRUE(mea::makeStatus(mea::StatusCode::WouldBlock, 1).transient());
    TEST_ASSERT_FALSE(mea::makeStatus(mea::StatusCode::Timeout, 1).transient());
    TEST_ASSERT_FALSE(mea::makeStatus(mea::StatusCode::IoError, 1).transient());
    TEST_ASSERT_FALSE(mea::okStatus().transient());
}

static void test_status_carries_origin_and_detail() {
    const mea::Status status = mea::makeStatus(mea::StatusCode::IoError, 42, 7);
    TEST_ASSERT_FALSE(status.ok());
    TEST_ASSERT_EQUAL_UINT16(42, status.origin);
    TEST_ASSERT_EQUAL_UINT16(7, status.detail);
}

static void test_status_code_names() {
    TEST_ASSERT_EQUAL_STRING("Ok", mea::statusCodeName(mea::StatusCode::Ok));
    TEST_ASSERT_EQUAL_STRING("WouldBlock",
                             mea::statusCodeName(mea::StatusCode::WouldBlock));
    TEST_ASSERT_EQUAL_STRING("InternalError",
                             mea::statusCodeName(mea::StatusCode::InternalError));
}

// ---------------------------------------------------------------- Zeit

static void test_elapsed_ms_plain() {
    TEST_ASSERT_EQUAL_UINT32(50, mea::elapsedMs(150, 100));
    TEST_ASSERT_TRUE(mea::intervalElapsed(150, 100, 50));
    TEST_ASSERT_FALSE(mea::intervalElapsed(149, 100, 50));
}

static void test_elapsed_ms_rollover() {
    const mea::TimestampMs before = std::numeric_limits<std::uint32_t>::max() - 10U;
    const mea::TimestampMs after = 20U;  // 31 ms später, über den Überlauf hinweg
    TEST_ASSERT_EQUAL_UINT32(31, mea::elapsedMs(after, before));
    TEST_ASSERT_TRUE(mea::intervalElapsed(after, before, 31));
    TEST_ASSERT_FALSE(mea::intervalElapsed(after, before, 32));
}

// ---------------------------------------------------------------- Measurement

static void test_measurement_validation() {
    mea::Measurement measurement{};
    TEST_ASSERT_FALSE(mea::isValid(measurement));  // ungültige ID

    measurement.sourceId = 5;
    measurement.value = 1.5F;
    TEST_ASSERT_TRUE(mea::isValid(measurement));

    measurement.value = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(mea::isValid(measurement));
    TEST_ASSERT_FALSE(mea::hasFiniteValue(measurement));

    measurement.value = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(mea::isValid(measurement));
}

static void test_quality_flag_bit_operations() {
    mea::QualityFlag quality = mea::QualityFlag::None;
    TEST_ASSERT_FALSE(mea::hasFlag(quality, mea::QualityFlag::Stale));
    TEST_ASSERT_FALSE(mea::hasFlag(quality, mea::QualityFlag::None));

    quality |= mea::QualityFlag::Stale;
    quality |= mea::QualityFlag::OutOfRange;
    TEST_ASSERT_TRUE(mea::hasFlag(quality, mea::QualityFlag::Stale));
    TEST_ASSERT_TRUE(mea::hasFlag(quality, mea::QualityFlag::OutOfRange));
    TEST_ASSERT_FALSE(mea::hasFlag(quality, mea::QualityFlag::SensorFault));
    TEST_ASSERT_TRUE(
        mea::hasFlag(quality, mea::QualityFlag::Stale | mea::QualityFlag::OutOfRange));
}

// ---------------------------------------------------------------- ArrayView

static void test_array_view_default_is_empty() {
    constexpr mea::ArrayView<mea::ComponentId> view;
    TEST_ASSERT_TRUE(view.empty());
    TEST_ASSERT_EQUAL_size_t(0, view.size());
    TEST_ASSERT_NULL(view.at(0));
}

static void test_array_view_access() {
    static constexpr mea::ComponentId ids[] = {10, 20, 30};
    constexpr mea::ArrayView<const mea::ComponentId> view(ids, 3);
    TEST_ASSERT_EQUAL_size_t(3, view.size());
    TEST_ASSERT_FALSE(view.empty());
    TEST_ASSERT_EQUAL_UINT16(20, view[1]);
    TEST_ASSERT_NOT_NULL(view.at(2));
    TEST_ASSERT_EQUAL_UINT16(30, *view.at(2));
    TEST_ASSERT_NULL(view.at(3));
}

static void test_array_view_null_data_is_empty() {
    constexpr mea::ArrayView<const mea::ComponentId> view(nullptr, 0);
    TEST_ASSERT_TRUE(view.empty());
    TEST_ASSERT_EQUAL_size_t(0, view.size());
}

// ---------------------------------------------------------------- RingBuffer

static void test_ring_buffer_push_pop_order() {
    mea::RingBuffer<std::uint32_t, 3> buffer;
    TEST_ASSERT_TRUE(buffer.empty());
    TEST_ASSERT_TRUE(buffer.push(1));
    TEST_ASSERT_TRUE(buffer.push(2));
    TEST_ASSERT_TRUE(buffer.push(3));
    TEST_ASSERT_TRUE(buffer.full());
    TEST_ASSERT_FALSE(buffer.push(4));  // voll: Element wird abgelehnt

    std::uint32_t out = 0;
    TEST_ASSERT_TRUE(buffer.pop(out));
    TEST_ASSERT_EQUAL_UINT32(1, out);
    TEST_ASSERT_TRUE(buffer.push(4));  // Platz wieder frei
    TEST_ASSERT_TRUE(buffer.pop(out));
    TEST_ASSERT_EQUAL_UINT32(2, out);
    TEST_ASSERT_TRUE(buffer.pop(out));
    TEST_ASSERT_EQUAL_UINT32(3, out);
    TEST_ASSERT_TRUE(buffer.pop(out));
    TEST_ASSERT_EQUAL_UINT32(4, out);
    TEST_ASSERT_FALSE(buffer.pop(out));  // leer
}

static void test_ring_buffer_front_and_clear() {
    mea::RingBuffer<std::uint32_t, 2> buffer;
    TEST_ASSERT_NULL(buffer.front());
    TEST_ASSERT_TRUE(buffer.push(7));
    TEST_ASSERT_NOT_NULL(buffer.front());
    TEST_ASSERT_EQUAL_UINT32(7, *buffer.front());
    TEST_ASSERT_EQUAL_size_t(1, buffer.size());  // front() entnimmt nicht
    buffer.clear();
    TEST_ASSERT_TRUE(buffer.empty());
}

// ---------------------------------------------------------------- Health

static void test_health_record_result() {
    mea::ComponentHealth health{};
    health.componentId = 9;

    mea::recordResult(health, mea::okStatus(), 100);
    TEST_ASSERT_EQUAL_UINT32(1, health.successCount);
    TEST_ASSERT_EQUAL_UINT32(0, health.errorCount);
    TEST_ASSERT_EQUAL_UINT32(100, health.lastSuccessMs);

    mea::recordResult(health, mea::makeStatus(mea::StatusCode::Busy, 9), 150);
    TEST_ASSERT_EQUAL_UINT32(0, health.errorCount);  // transient zählt nicht

    mea::recordResult(health, mea::makeStatus(mea::StatusCode::IoError, 9), 200);
    TEST_ASSERT_EQUAL_UINT32(1, health.errorCount);
    TEST_ASSERT_EQUAL_UINT32(100, health.lastSuccessMs);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::IoError),
                            static_cast<std::uint8_t>(health.lastStatus.code));
}

// ---------------------------------------------------------------- Command

static void test_command_defaults() {
    constexpr mea::Command command{};
    TEST_ASSERT_EQUAL_UINT16(mea::InvalidComponentId, command.targetId);
    TEST_ASSERT_EQUAL_UINT16(static_cast<std::uint16_t>(mea::CommandType::Unknown),
                             static_cast<std::uint16_t>(command.type));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_status_default_is_ok);
    RUN_TEST(test_status_transient_codes);
    RUN_TEST(test_status_carries_origin_and_detail);
    RUN_TEST(test_status_code_names);
    RUN_TEST(test_elapsed_ms_plain);
    RUN_TEST(test_elapsed_ms_rollover);
    RUN_TEST(test_measurement_validation);
    RUN_TEST(test_quality_flag_bit_operations);
    RUN_TEST(test_array_view_default_is_empty);
    RUN_TEST(test_array_view_access);
    RUN_TEST(test_array_view_null_data_is_empty);
    RUN_TEST(test_ring_buffer_push_pop_order);
    RUN_TEST(test_ring_buffer_front_and_clear);
    RUN_TEST(test_health_record_result);
    RUN_TEST(test_command_defaults);
    return UNITY_END();
}
