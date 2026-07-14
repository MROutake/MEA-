#include <unity.h>

#include <cstdint>
#include <limits>

#include <MeaProcessing.h>
#include <mea/testing/ContractChecks.h>

void setUp() {}
void tearDown() {}

namespace {

mea::Measurement makeInput(
    const float value, const mea::MeasurementKind kind = mea::MeasurementKind::RawAnalog,
    const mea::Unit unit = mea::Unit::RawCount) {
    mea::Measurement input{};
    input.sourceId = 7;
    input.kind = kind;
    input.unit = unit;
    input.value = value;
    input.sampledAtMs = 100;
    input.sequence = 5;
    return input;
}

}  // namespace

// ---------------------------------------------------------------- Contracts

static void test_processors_fulfill_pre_begin_contract() {
    mea::PassThroughProcessor passThrough(1);
    mea::LinearProcessor linear({2, 1.0F, 0.0F});
    mea::ClampProcessor clamp({3, 0.0F, 1.0F});
    mea::RangeValidationProcessor range({4, 0.0F, 1.0F});
    mea::MovingAverageProcessor<4> average({5});

    TEST_ASSERT_NULL(mea::testing::checkProcessorPreBeginContract(passThrough));
    TEST_ASSERT_NULL(mea::testing::checkProcessorPreBeginContract(linear));
    TEST_ASSERT_NULL(mea::testing::checkProcessorPreBeginContract(clamp));
    TEST_ASSERT_NULL(mea::testing::checkProcessorPreBeginContract(range));
    TEST_ASSERT_NULL(mea::testing::checkProcessorPreBeginContract(average));
}

static void test_invalid_ids_are_rejected() {
    mea::PassThroughProcessor passThrough(mea::InvalidComponentId);
    mea::LinearProcessor linear({mea::InvalidComponentId, 1.0F, 0.0F});
    mea::MovingAverageProcessor<2> average({mea::InvalidComponentId});
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidConfiguration),
        static_cast<std::uint8_t>(passThrough.begin().code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidConfiguration),
        static_cast<std::uint8_t>(linear.begin().code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidConfiguration),
        static_cast<std::uint8_t>(average.begin().code));
}

// ---------------------------------------------------------------- Linear

static void test_linear_conversion_and_output_unit() {
    mea::LinearProcessor::Config config{};
    config.processorId = 2;
    config.gain = 2.0F;
    config.offset = -1.0F;
    config.inputKind = mea::MeasurementKind::RawAnalog;
    config.inputUnit = mea::Unit::RawCount;
    config.outputKind = mea::MeasurementKind::Voltage;
    config.outputUnit = mea::Unit::Volt;
    mea::LinearProcessor processor(config);
    TEST_ASSERT_TRUE(processor.begin().ok());

    mea::Measurement output{};
    TEST_ASSERT_TRUE(processor.process(makeInput(3.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 5.0F, output.value);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::MeasurementKind::Voltage),
                            static_cast<std::uint8_t>(output.kind));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::Unit::Volt),
                            static_cast<std::uint8_t>(output.unit));
    TEST_ASSERT_EQUAL_UINT32(5, output.sequence);  // Metadaten bleiben erhalten
    TEST_ASSERT_EQUAL_UINT32(100, output.sampledAtMs);
}

static void test_linear_rejects_wrong_input_kind() {
    mea::LinearProcessor::Config config{};
    config.processorId = 2;
    config.inputKind = mea::MeasurementKind::RawAnalog;
    config.inputUnit = mea::Unit::RawCount;
    mea::LinearProcessor processor(config);
    TEST_ASSERT_TRUE(processor.begin().ok());
    TEST_ASSERT_FALSE(
        processor.accepts(mea::MeasurementKind::Temperature, mea::Unit::DegreeCelsius));

    mea::Measurement output{};
    const mea::Status status = processor.process(
        makeInput(1.0F, mea::MeasurementKind::Temperature, mea::Unit::DegreeCelsius),
        output);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::Unsupported),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(2, status.origin);
}

static void test_linear_rejects_nan_input() {
    mea::LinearProcessor processor({2, 1.0F, 0.0F});
    TEST_ASSERT_TRUE(processor.begin().ok());
    mea::Measurement output{};
    const mea::Status status =
        processor.process(makeInput(std::numeric_limits<float>::quiet_NaN()), output);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::InvalidArgument),
                            static_cast<std::uint8_t>(status.code));
}

static void test_linear_reports_non_finite_result() {
    mea::LinearProcessor processor({2, std::numeric_limits<float>::max(), 0.0F});
    TEST_ASSERT_TRUE(processor.begin().ok());
    mea::Measurement output{};
    const mea::Status status =
        processor.process(makeInput(std::numeric_limits<float>::max()), output);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::ProcessingError),
                            static_cast<std::uint8_t>(status.code));
}

// ---------------------------------------------------------------- Clamp

static void test_clamp_limits_and_flags() {
    mea::ClampProcessor processor({3, 0.0F, 3.3F});
    TEST_ASSERT_TRUE(processor.begin().ok());

    mea::Measurement output{};
    TEST_ASSERT_TRUE(processor.process(makeInput(1.5F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.5F, output.value);
    TEST_ASSERT_FALSE(mea::hasFlag(output.quality, mea::QualityFlag::OutOfRange));

    TEST_ASSERT_TRUE(processor.process(makeInput(5.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 3.3F, output.value);
    TEST_ASSERT_TRUE(mea::hasFlag(output.quality, mea::QualityFlag::OutOfRange));

    TEST_ASSERT_TRUE(processor.process(makeInput(-1.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, output.value);
    TEST_ASSERT_TRUE(mea::hasFlag(output.quality, mea::QualityFlag::OutOfRange));
}

static void test_clamp_rejects_inverted_range() {
    mea::ClampProcessor processor({3, 2.0F, 1.0F});
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidConfiguration),
        static_cast<std::uint8_t>(processor.begin().code));
}

// ---------------------------------------------------------------- RangeValidation

static void test_range_validation_flags_without_changing_value() {
    mea::RangeValidationProcessor processor({4, 0.0F, 3.3F});
    TEST_ASSERT_TRUE(processor.begin().ok());

    mea::Measurement output{};
    TEST_ASSERT_TRUE(processor.process(makeInput(5.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 5.0F, output.value);  // Wert unverändert
    TEST_ASSERT_TRUE(mea::hasFlag(output.quality, mea::QualityFlag::OutOfRange));

    TEST_ASSERT_TRUE(processor.process(makeInput(2.0F), output).ok());
    TEST_ASSERT_FALSE(mea::hasFlag(output.quality, mea::QualityFlag::OutOfRange));
}

// ---------------------------------------------------------------- MovingAverage

static void test_moving_average_warmup_is_estimated() {
    mea::MovingAverageProcessor<4> processor({5});
    TEST_ASSERT_TRUE(processor.begin().ok());

    mea::Measurement output{};
    TEST_ASSERT_TRUE(processor.process(makeInput(2.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 2.0F, output.value);
    TEST_ASSERT_TRUE(mea::hasFlag(output.quality, mea::QualityFlag::Estimated));

    TEST_ASSERT_TRUE(processor.process(makeInput(4.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 3.0F, output.value);
    TEST_ASSERT_TRUE(mea::hasFlag(output.quality, mea::QualityFlag::Estimated));
}

static void test_moving_average_full_window() {
    mea::MovingAverageProcessor<3> processor({5});
    TEST_ASSERT_TRUE(processor.begin().ok());

    mea::Measurement output{};
    TEST_ASSERT_TRUE(processor.process(makeInput(1.0F), output).ok());
    TEST_ASSERT_TRUE(processor.process(makeInput(2.0F), output).ok());
    TEST_ASSERT_TRUE(processor.process(makeInput(3.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 2.0F, output.value);
    TEST_ASSERT_FALSE(mea::hasFlag(output.quality, mea::QualityFlag::Estimated));

    // Ältester Wert (1.0) fällt heraus: (2+3+7)/3 = 4
    TEST_ASSERT_TRUE(processor.process(makeInput(7.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 4.0F, output.value);
    TEST_ASSERT_FALSE(mea::hasFlag(output.quality, mea::QualityFlag::Estimated));
}

static void test_moving_average_reset() {
    mea::MovingAverageProcessor<2> processor({5});
    TEST_ASSERT_TRUE(processor.begin().ok());

    mea::Measurement output{};
    TEST_ASSERT_TRUE(processor.process(makeInput(10.0F), output).ok());
    TEST_ASSERT_TRUE(processor.process(makeInput(20.0F), output).ok());
    TEST_ASSERT_EQUAL_size_t(2, processor.filled());

    processor.reset();
    TEST_ASSERT_EQUAL_size_t(0, processor.filled());
    TEST_ASSERT_TRUE(processor.process(makeInput(6.0F), output).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 6.0F, output.value);
    TEST_ASSERT_TRUE(mea::hasFlag(output.quality, mea::QualityFlag::Estimated));
}

static void test_moving_average_rejects_invalid_values() {
    mea::MovingAverageProcessor<2> processor({5});
    TEST_ASSERT_TRUE(processor.begin().ok());

    mea::Measurement output{};
    const mea::Status status =
        processor.process(makeInput(std::numeric_limits<float>::infinity()), output);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::InvalidArgument),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_size_t(0, processor.filled());  // Fenster unverändert
}

static void test_moving_average_keeps_timestamps_and_sequence() {
    mea::MovingAverageProcessor<2> processor({5});
    TEST_ASSERT_TRUE(processor.begin().ok());

    mea::Measurement first = makeInput(1.0F);
    first.sampledAtMs = 1000;
    first.sequence = 41;
    mea::Measurement second = makeInput(2.0F);
    second.sampledAtMs = 2000;
    second.sequence = 42;

    mea::Measurement output{};
    TEST_ASSERT_TRUE(processor.process(first, output).ok());
    TEST_ASSERT_EQUAL_UINT32(1000, output.sampledAtMs);
    TEST_ASSERT_EQUAL_UINT32(41, output.sequence);
    TEST_ASSERT_TRUE(processor.process(second, output).ok());
    TEST_ASSERT_EQUAL_UINT32(2000, output.sampledAtMs);
    TEST_ASSERT_EQUAL_UINT32(42, output.sequence);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_processors_fulfill_pre_begin_contract);
    RUN_TEST(test_invalid_ids_are_rejected);
    RUN_TEST(test_linear_conversion_and_output_unit);
    RUN_TEST(test_linear_rejects_wrong_input_kind);
    RUN_TEST(test_linear_rejects_nan_input);
    RUN_TEST(test_linear_reports_non_finite_result);
    RUN_TEST(test_clamp_limits_and_flags);
    RUN_TEST(test_clamp_rejects_inverted_range);
    RUN_TEST(test_range_validation_flags_without_changing_value);
    RUN_TEST(test_moving_average_warmup_is_estimated);
    RUN_TEST(test_moving_average_full_window);
    RUN_TEST(test_moving_average_reset);
    RUN_TEST(test_moving_average_rejects_invalid_values);
    RUN_TEST(test_moving_average_keeps_timestamps_and_sequence);
    return UNITY_END();
}
