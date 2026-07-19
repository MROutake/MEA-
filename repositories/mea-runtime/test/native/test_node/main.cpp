#include <unity.h>

#include <cstdint>

#include <MeaRuntime.h>

void setUp() {}
void tearDown() {}

namespace {

// ---------------------------------------------------------------- Fakes

class FakeDevice final : public mea::IDevice {
public:
    mea::Status begin() noexcept override {
        ++beginCalls;
        return beginResult;
    }
    mea::Status update(mea::TimestampMs) noexcept override {
        ++updateCalls;
        return mea::okStatus();
    }

    mea::Status beginResult{mea::okStatus()};
    std::uint32_t beginCalls{0};
    std::uint32_t updateCalls{0};
};

class FakeSource final : public mea::IMeasurementSource {
public:
    explicit FakeSource(const mea::ComponentId id) noexcept : id_(id) {}

    [[nodiscard]] mea::ComponentId id() const noexcept override { return id_; }
    mea::Status begin() noexcept override {
        ++beginCalls;
        return beginResult;
    }
    mea::Status update(mea::TimestampMs) noexcept override { return mea::okStatus(); }
    [[nodiscard]] std::size_t available() const noexcept override {
        return queue_.size();
    }
    mea::Status read(mea::Measurement& output) noexcept override {
        if (!queue_.pop(output)) {
            return mea::makeStatus(mea::StatusCode::NoData, id_);
        }
        return mea::okStatus();
    }

    void push(const float value) noexcept {
        mea::Measurement measurement{};
        measurement.sourceId = id_;
        measurement.kind = mea::MeasurementKind::Temperature;
        measurement.unit = mea::Unit::DegreeCelsius;
        measurement.value = value;
        measurement.sequence = ++sequence_;
        (void)queue_.push(measurement);
    }

    mea::Status beginResult{mea::okStatus()};
    std::uint32_t beginCalls{0};

private:
    mea::ComponentId id_;
    mea::RingBuffer<mea::Measurement, 4> queue_{};
    mea::SequenceNumber sequence_{0};
};

class FakeProcessor final : public mea::IMeasurementProcessor {
public:
    explicit FakeProcessor(const mea::ComponentId id, const float offset) noexcept
        : id_(id), offset_(offset) {}

    [[nodiscard]] mea::ComponentId id() const noexcept override { return id_; }
    mea::Status begin() noexcept override {
        ++beginCalls;
        return mea::okStatus();
    }
    [[nodiscard]] bool accepts(mea::MeasurementKind, mea::Unit) const noexcept override {
        return true;
    }
    mea::Status process(const mea::Measurement& input,
                        mea::Measurement& output) noexcept override {
        output = input;
        output.value = input.value + offset_;
        return mea::okStatus();
    }

    std::uint32_t beginCalls{0};

private:
    mea::ComponentId id_;
    float offset_;
};

class FakeSink final : public mea::IMeasurementSink {
public:
    explicit FakeSink(const mea::ComponentId id) noexcept : id_(id) {}

    [[nodiscard]] mea::ComponentId id() const noexcept override { return id_; }
    mea::Status begin() noexcept override {
        ++beginCalls;
        return mea::okStatus();
    }
    mea::Status update(mea::TimestampMs) noexcept override { return mea::okStatus(); }
    [[nodiscard]] std::size_t capacityAvailable() const noexcept override { return 16; }
    mea::Status submit(const mea::Measurement& measurement) noexcept override {
        last = measurement;
        ++submitted;
        return mea::okStatus();
    }

    mea::Measurement last{};
    std::uint32_t submitted{0};
    std::uint32_t beginCalls{0};

private:
    mea::ComponentId id_;
};

// ---------------------------------------------------------------- Helfer

using TestNode = mea::MeasurementNode<4, 4, 2, 3, 2>;

/// Treibt den Node an, bis der Sink @p expected Messwerte hat (max. Ticks).
void updateUntilSubmitted(TestNode& node, const FakeSink& sink,
                          const std::uint32_t expected, mea::TimestampMs& nowMs) {
    for (std::uint32_t tick = 0; tick < 64 && sink.submitted < expected; ++tick) {
        (void)node.update(nowMs);
        nowMs += 100;
    }
}

std::uint32_t reporterCalls = 0;
const char* reporterLastStage = nullptr;
mea::Status reporterLastStatus{};

void testReporter(const char* stage, const mea::Status& status) {
    ++reporterCalls;
    reporterLastStage = stage;
    reporterLastStatus = status;
}

void resetReporter() {
    reporterCalls = 0;
    reporterLastStage = nullptr;
    reporterLastStatus = mea::okStatus();
}

constexpr mea::PipelineTuning kFastTuning{100, 1000, 1000, {0, 0}, true};

}  // namespace

// ---------------------------------------------------------------- Aufbau

static void test_happy_path_measurement_flows_to_sink() {
    FakeDevice device;
    FakeSource source(100);
    FakeProcessor processor(200, 1.0F);
    FakeSink sink(300);

    TestNode node;
    node.setDefaultTuning(kFastTuning);
    node.addDevice(device);
    node.addPipeline(400, source).through(processor).into(sink);

    TEST_ASSERT_TRUE(node.begin(0).ok());
    TEST_ASSERT_EQUAL_UINT32(1, device.beginCalls);
    TEST_ASSERT_EQUAL_size_t(1, node.activePipelines());

    source.push(20.0F);
    mea::TimestampMs now = 100;
    updateUntilSubmitted(node, sink, 1, now);

    TEST_ASSERT_EQUAL_UINT32(1, sink.submitted);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 21.0F, sink.last.value);  // Prozessor +1
    TEST_ASSERT_EQUAL_UINT16(100, sink.last.sourceId);
    TEST_ASSERT_GREATER_THAN_UINT32(0, device.updateCalls);
}

static void test_shared_sink_and_processor_registered_once() {
    FakeSource sourceA(100);
    FakeSource sourceB(101);
    FakeProcessor processor(200, 1.0F);
    FakeSink sink(300);

    TestNode node;
    node.setDefaultTuning(kFastTuning);
    node.addPipeline(400, sourceA).through(processor).into(sink);
    node.addPipeline(401, sourceB).through(processor).into(sink);

    TEST_ASSERT_TRUE(node.begin(0).ok());
    TEST_ASSERT_EQUAL_size_t(2, node.activePipelines());
    TEST_ASSERT_EQUAL_UINT32(1, processor.beginCalls);  // nur einmal registriert
    TEST_ASSERT_EQUAL_UINT32(1, sink.beginCalls);

    sourceA.push(10.0F);
    sourceB.push(30.0F);
    mea::TimestampMs now = 100;
    updateUntilSubmitted(node, sink, 2, now);
    TEST_ASSERT_EQUAL_UINT32(2, sink.submitted);
}

static void test_duplicate_pipeline_id_blocks_begin() {
    FakeSource sourceA(100);
    FakeSource sourceB(101);
    FakeSink sink(300);

    TestNode node;
    node.addPipeline(400, sourceA).into(sink);
    node.addPipeline(400, sourceB).into(sink);  // doppelte Pipeline-ID

    const mea::Status status = node.begin(0);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::DuplicateId),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::NotInitialized),
        static_cast<std::uint8_t>(node.update(0).code));
}

static void test_foreign_component_with_same_id_blocks_begin() {
    FakeSource sourceA(100);
    FakeSource sourceB(100);  // anderes Objekt, gleiche ID
    FakeSink sink(300);

    TestNode node;
    node.addPipeline(400, sourceA).into(sink);
    node.addPipeline(401, sourceB).into(sink);

    const mea::Status status = node.begin(0);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::DuplicateId),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(100, status.origin);
}

static void test_pipeline_capacity_is_enforced() {
    FakeSource sources[] = {FakeSource(100), FakeSource(101), FakeSource(102),
                            FakeSource(103)};
    FakeSink sink(300);

    TestNode node;  // MaxPipelines = 3
    node.addPipeline(400, sources[0]).into(sink);
    node.addPipeline(401, sources[1]).into(sink);
    node.addPipeline(402, sources[2]).into(sink);
    node.addPipeline(403, sources[3]).into(sink);  // über Kapazität

    const mea::Status status = node.begin(0);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::CapacityExceeded),
                            static_cast<std::uint8_t>(status.code));
}

static void test_missing_sink_fails_pipeline_begin() {
    FakeSource source(100);

    TestNode node;
    node.addPipeline(400, source);  // kein into(): ungültig (ADR 0005)

    const mea::Status status = node.begin(0);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidConfiguration),
        static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_size_t(0, node.activePipelines());
}

// ---------------------------------------------------------------- Degradation

static void test_failed_source_disables_only_its_pipeline() {
    resetReporter();
    FakeDevice brokenDevice;
    brokenDevice.beginResult =
        mea::makeStatus(mea::StatusCode::IoError, mea::InvalidComponentId, 7);
    FakeSource healthySource(100);
    FakeSource brokenSource(101);  // Sensor am ausgefallenen Gerät
    brokenSource.beginResult =
        mea::makeStatus(mea::StatusCode::NotInitialized, 101);
    FakeSink sink(300);

    TestNode node;
    node.setReporter(&testReporter);
    node.setDefaultTuning(kFastTuning);
    node.addDevice(brokenDevice);
    node.addPipeline(400, healthySource).into(sink);
    node.addPipeline(401, brokenSource).into(sink);

    TEST_ASSERT_TRUE(node.begin(0).ok());  // degradiert, aber lauffähig
    TEST_ASSERT_EQUAL_size_t(1, node.activePipelines());
    TEST_ASSERT_GREATER_THAN_UINT32(1, reporterCalls);  // Gerät + Quelle gemeldet

    healthySource.push(20.0F);
    mea::TimestampMs now = 100;
    updateUntilSubmitted(node, sink, 1, now);
    TEST_ASSERT_EQUAL_UINT32(1, sink.submitted);
}

static void test_reporter_receives_device_failure() {
    resetReporter();
    FakeDevice device;
    device.beginResult = mea::makeStatus(mea::StatusCode::IoError,
                                         mea::InvalidComponentId, 3);
    FakeSource source(100);
    FakeSink sink(300);

    TestNode node;
    node.setReporter(&testReporter);
    node.addDevice(device);
    node.addPipeline(400, source).into(sink);

    TEST_ASSERT_TRUE(node.begin(0).ok());
    TEST_ASSERT_EQUAL_UINT32(1, reporterCalls);
    TEST_ASSERT_EQUAL_STRING("device begin", reporterLastStage);
    TEST_ASSERT_EQUAL_UINT16(3, reporterLastStatus.detail);
}

static void test_update_before_begin_reports_not_initialized() {
    TestNode node;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::NotInitialized),
        static_cast<std::uint8_t>(node.update(0).code));
}

static void test_pipeline_diagnostics_are_accessible() {
    FakeSource source(100);
    FakeSink sink(300);

    TestNode node;
    node.setDefaultTuning(kFastTuning);
    node.addPipeline(400, source).into(sink);
    TEST_ASSERT_TRUE(node.begin(0).ok());

    source.push(1.0F);
    mea::TimestampMs now = 100;
    updateUntilSubmitted(node, sink, 1, now);

    const mea::MeasurementPipelineMachine* pipeline = node.pipeline(400);
    TEST_ASSERT_NOT_NULL(pipeline);
    TEST_ASSERT_EQUAL_UINT32(1, pipeline->completedCycles());
    TEST_ASSERT_NULL(node.pipeline(999));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_happy_path_measurement_flows_to_sink);
    RUN_TEST(test_shared_sink_and_processor_registered_once);
    RUN_TEST(test_duplicate_pipeline_id_blocks_begin);
    RUN_TEST(test_foreign_component_with_same_id_blocks_begin);
    RUN_TEST(test_pipeline_capacity_is_enforced);
    RUN_TEST(test_missing_sink_fails_pipeline_begin);
    RUN_TEST(test_failed_source_disables_only_its_pipeline);
    RUN_TEST(test_reporter_receives_device_failure);
    RUN_TEST(test_update_before_begin_reports_not_initialized);
    RUN_TEST(test_pipeline_diagnostics_are_accessible);
    return UNITY_END();
}
