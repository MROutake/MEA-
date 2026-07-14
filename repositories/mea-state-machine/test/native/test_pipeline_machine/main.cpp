#include <unity.h>

#include <cstdint>
#include <limits>

#include <MeaManagers.h>
#include <MeaStateMachine.h>

void setUp() {}
void tearDown() {}

namespace {

// ------------------------------------------------------------ Fakes

class FakeSource final : public mea::IMeasurementSource {
public:
    explicit FakeSource(const mea::ComponentId id) noexcept : id_(id) {}

    [[nodiscard]] mea::ComponentId id() const noexcept override { return id_; }

    mea::Status begin() noexcept override {
        ++beginCalls;
        return mea::okStatus();
    }

    mea::Status update(mea::TimestampMs) noexcept override { return mea::okStatus(); }

    [[nodiscard]] std::size_t available() const noexcept override {
        return hasData_ ? 1U : 0U;
    }

    mea::Status read(mea::Measurement& output) noexcept override {
        if (!nextReadStatus.ok()) {
            const mea::Status status = nextReadStatus;  // einmalig injizierter Fehler
            nextReadStatus = mea::okStatus();
            return status;
        }
        if (!hasData_) {
            return mea::makeStatus(mea::StatusCode::NoData, id_);
        }
        output = pending_;
        hasData_ = false;
        return mea::okStatus();
    }

    void seed(const float value, const mea::TimestampMs sampledAtMs) noexcept {
        pending_ = mea::Measurement{};
        pending_.sourceId = id_;
        pending_.kind = mea::MeasurementKind::RawAnalog;
        pending_.unit = mea::Unit::RawCount;
        pending_.value = value;
        pending_.sampledAtMs = sampledAtMs;
        pending_.sequence = ++sequence_;
        hasData_ = true;
    }

    mea::Status nextReadStatus{mea::okStatus()};
    std::uint32_t beginCalls{0};

private:
    mea::ComponentId id_;
    mea::Measurement pending_{};
    mea::SequenceNumber sequence_{0};
    bool hasData_{false};
};

class FakeProcessor final : public mea::IMeasurementProcessor {
public:
    FakeProcessor(const mea::ComponentId id, const float factor) noexcept
        : id_(id), factor_(factor) {}

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
        ++processCalls;
        if (failCount > 0) {
            --failCount;
            return failStatus;
        }
        output = input;
        output.value = input.value * factor_;
        return mea::okStatus();
    }

    std::uint32_t beginCalls{0};
    std::uint32_t processCalls{0};
    std::uint32_t failCount{0};
    mea::Status failStatus{mea::makeStatus(mea::StatusCode::ProcessingError, 0)};

private:
    mea::ComponentId id_;
    float factor_;
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

    [[nodiscard]] std::size_t capacityAvailable() const noexcept override { return 1; }

    mea::Status submit(const mea::Measurement& measurement) noexcept override {
        ++submitCalls;
        if (failCount > 0) {
            --failCount;
            mea::Status status = failStatus;
            status.origin = id_;
            return status;
        }
        last = measurement;
        ++accepted;
        return mea::okStatus();
    }

    std::uint32_t beginCalls{0};
    std::uint32_t submitCalls{0};
    std::uint32_t accepted{0};
    std::uint32_t failCount{0};
    mea::Status failStatus{mea::makeStatus(mea::StatusCode::WouldBlock, 0)};
    mea::Measurement last{};

private:
    mea::ComponentId id_;
};

// ------------------------------------------------------------ Fixture

constexpr mea::ComponentId kSourceId = 10;
constexpr mea::ComponentId kProc1 = 20;
constexpr mea::ComponentId kProc2 = 21;
constexpr mea::ComponentId kSink1 = 30;
constexpr mea::ComponentId kSink2 = 31;

constexpr mea::ComponentId kOneProcessor[] = {kProc1};
constexpr mea::ComponentId kTwoProcessors[] = {kProc1, kProc2};
constexpr mea::ComponentId kDuplicateProcessors[] = {kProc1, kProc1};
constexpr mea::ComponentId kOneSink[] = {kSink1};
constexpr mea::ComponentId kTwoSinks[] = {kSink2, kSink1};  // s2 zuerst (Masken-Test)

struct Fixture {
    FakeSource source{kSourceId};
    FakeProcessor processor1{kProc1, 2.0F};
    FakeProcessor processor2{kProc2, 10.0F};
    FakeSink sink1{kSink1};
    FakeSink sink2{kSink2};

    mea::SensorManager<2> sources;
    mea::ProcessorManager<4> processors;
    mea::SinkManager<4> sinks;

    void wire() {
        TEST_ASSERT_TRUE(sources.registerComponent(source).ok());
        TEST_ASSERT_TRUE(processors.registerComponent(processor1).ok());
        TEST_ASSERT_TRUE(processors.registerComponent(processor2).ok());
        TEST_ASSERT_TRUE(sinks.registerComponent(sink1).ok());
        TEST_ASSERT_TRUE(sinks.registerComponent(sink2).ok());
        TEST_ASSERT_TRUE(sources.beginAll().ok());
        TEST_ASSERT_TRUE(processors.beginAll().ok());
        TEST_ASSERT_TRUE(sinks.beginAll().ok());
    }
};

mea::PipelineConfig baseConfig() {
    mea::PipelineConfig config{};
    config.pipelineId = 1;
    config.sourceId = kSourceId;
    config.processorIds = mea::ArrayView<const mea::ComponentId>(kOneProcessor, 1);
    config.sinkIds = mea::ArrayView<const mea::ComponentId>(kOneSink, 1);
    config.cycleIntervalMs = 100;
    config.acquisitionTimeoutMs = 50;
    config.publishTimeoutMs = 50;
    config.retry = {10, 0};
    config.startImmediately = true;
    return config;
}

/// Ruft update() für jeden Zeitschritt in [from, to] auf.
void run(mea::MeasurementPipelineMachine& machine, const mea::TimestampMs from,
         const mea::TimestampMs to) {
    for (mea::TimestampMs now = from; now != to + 1U; ++now) {
        (void)machine.update(now);
    }
}

#define ASSERT_CODE(expected, status)                                             \
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::expected), \
                            static_cast<std::uint8_t>((status).code))

}  // namespace

// ------------------------------------------------------------ Konfiguration

static void test_update_before_begin_reports_not_initialized() {
    Fixture fixture;
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    ASSERT_CODE(NotInitialized, machine.update(0));
}

static void test_invalid_configuration_is_rejected() {
    Fixture fixture;
    fixture.wire();

    auto noInterval = baseConfig();
    noInterval.cycleIntervalMs = 0;
    mea::MeasurementPipelineMachine machine1(fixture.sources, fixture.processors,
                                             fixture.sinks, noInterval);
    ASSERT_CODE(InvalidConfiguration, machine1.begin(0));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::PipelineState::Fault),
                            static_cast<std::uint8_t>(machine1.state()));

    auto noSinks = baseConfig();
    noSinks.sinkIds = mea::ArrayView<const mea::ComponentId>();
    mea::MeasurementPipelineMachine machine2(fixture.sources, fixture.processors,
                                             fixture.sinks, noSinks);
    ASSERT_CODE(InvalidConfiguration, machine2.begin(0));

    auto noPipelineId = baseConfig();
    noPipelineId.pipelineId = mea::InvalidComponentId;
    mea::MeasurementPipelineMachine machine3(fixture.sources, fixture.processors,
                                             fixture.sinks, noPipelineId);
    ASSERT_CODE(InvalidConfiguration, machine3.begin(0));
}

static void test_duplicate_ids_are_rejected() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.processorIds = mea::ArrayView<const mea::ComponentId>(kDuplicateProcessors, 2);
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    const mea::Status status = machine.begin(0);
    ASSERT_CODE(DuplicateId, status);
    TEST_ASSERT_EQUAL_UINT16(kProc1, status.detail);
}

static void test_missing_component_id_is_reported() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.sourceId = 99;  // nicht registriert
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    const mea::Status status = machine.begin(0);
    ASSERT_CODE(NotFound, status);
    TEST_ASSERT_EQUAL_UINT16(99, status.detail);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::PipelineState::Fault),
                            static_cast<std::uint8_t>(machine.state()));
}

// ------------------------------------------------------------ Normalbetrieb

static void test_happy_path_completes_cycle() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.source.seed(3.0F, 0);
    run(machine, 0, 5);

    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(0, machine.failedCycles());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sink1.accepted);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 6.0F, fixture.sink1.last.value);  // 3 * 2
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 6.0F, machine.lastMeasurement().value);
    TEST_ASSERT_TRUE(machine.lastStatus().ok());

    // Nächster Zyklus erst nach cycleIntervalMs.
    fixture.source.seed(4.0F, 50);
    run(machine, 6, 99);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    run(machine, 100, 110);
    TEST_ASSERT_EQUAL_UINT32(2, machine.completedCycles());
}

static void test_start_not_immediately_waits_for_enable() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.startImmediately = false;
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    TEST_ASSERT_TRUE(machine.begin(0).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::PipelineState::Disabled),
                            static_cast<std::uint8_t>(machine.state()));

    fixture.source.seed(1.0F, 0);
    run(machine, 0, 20);
    TEST_ASSERT_EQUAL_UINT32(0, machine.completedCycles());

    TEST_ASSERT_TRUE(machine.enable(21).ok());
    run(machine, 21, 30);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
}

static void test_source_busy_keeps_waiting() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.source.seed(1.0F, 0);
    fixture.source.nextReadStatus = mea::makeStatus(mea::StatusCode::Busy, kSourceId);
    TEST_ASSERT_TRUE(machine.update(0).ok());  // Zyklus startet
    TEST_ASSERT_TRUE(machine.update(1).ok());  // read -> Busy: weiter warten
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::PipelineState::WaitingForMeasurement),
        static_cast<std::uint8_t>(machine.state()));

    run(machine, 2, 6);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
}

static void test_source_no_data_race_keeps_waiting() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.source.seed(1.0F, 0);
    fixture.source.nextReadStatus = mea::makeStatus(mea::StatusCode::NoData, kSourceId);
    run(machine, 0, 6);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(0, machine.failedCycles());
}

static void test_acquisition_timeout_fails_cycle() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    run(machine, 0, 60);  // keine Daten, acquisitionTimeoutMs = 50
    TEST_ASSERT_EQUAL_UINT32(0, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(1, machine.failedCycles());
    ASSERT_CODE(Timeout, machine.lastStatus());
    TEST_ASSERT_EQUAL_UINT16(kSourceId, machine.lastStatus().detail);

    // Pipeline läuft weiter: nächster Zyklus kann erfolgreich sein.
    fixture.source.seed(1.0F, 150);
    run(machine, 61, 170);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
}

static void test_processor_error_fails_cycle() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.processor1.failCount = 1;
    fixture.processor1.failStatus =
        mea::makeStatus(mea::StatusCode::ProcessingError, kProc1);
    fixture.source.seed(1.0F, 0);
    run(machine, 0, 10);

    TEST_ASSERT_EQUAL_UINT32(0, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(1, machine.failedCycles());
    ASSERT_CODE(ProcessingError, machine.lastStatus());
    TEST_ASSERT_EQUAL_UINT16(kProc1, machine.lastStatus().origin);
    TEST_ASSERT_EQUAL_UINT32(0, fixture.sink1.submitCalls);
}

static void test_multiple_processors_run_in_order() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.processorIds = mea::ArrayView<const mea::ComponentId>(kTwoProcessors, 2);
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.source.seed(3.0F, 0);
    run(machine, 0, 6);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 60.0F, fixture.sink1.last.value);  // 3 * 2 * 10
}

static void test_multiple_sinks_all_receive() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.sinkIds = mea::ArrayView<const mea::ComponentId>(kTwoSinks, 2);
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.source.seed(3.0F, 0);
    run(machine, 0, 6);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sink1.accepted);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sink2.accepted);
}

// ------------------------------------------------------------ Backpressure

static void test_sink_would_block_enters_backpressure_and_recovers() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.sinkIds = mea::ArrayView<const mea::ComponentId>(kTwoSinks, 2);
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.sink1.failCount = 2;  // WouldBlock, dann frei
    fixture.source.seed(3.0F, 0);
    run(machine, 0, 3);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::PipelineState::Backpressure),
                            static_cast<std::uint8_t>(machine.state()));

    run(machine, 4, 10);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    // Der blockierte Sink hat die anderen nicht blockiert und nichts doppelt bekommen.
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sink2.accepted);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sink2.submitCalls);
}

static void test_publish_timeout_drops_measurement() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.sink1.failCount = 1000;  // dauerhaft WouldBlock
    fixture.source.seed(3.0F, 0);
    run(machine, 0, 60);  // publishTimeoutMs = 50, keine Retries

    TEST_ASSERT_EQUAL_UINT32(0, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(1, machine.failedCycles());
    TEST_ASSERT_EQUAL_UINT32(1, machine.droppedMeasurements());
    ASSERT_CODE(Timeout, machine.lastStatus());
}

// ------------------------------------------------------------ Retry

static void test_temporary_sink_error_is_retried_successfully() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.sinkIds = mea::ArrayView<const mea::ComponentId>(kTwoSinks, 2);
    config.retry = {10, 2};
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    // kTwoSinks = {sink2, sink1}: sink2 übernimmt sofort, sink1 scheitert einmal hart.
    fixture.sink1.failCount = 1;
    fixture.sink1.failStatus = mea::makeStatus(mea::StatusCode::IoError, kSink1);
    fixture.source.seed(3.0F, 0);
    run(machine, 0, 30);

    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(0, machine.failedCycles());
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sink1.accepted);
    // Bereits übernommene Sinks werden beim Retry nicht erneut beliefert.
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sink2.submitCalls);
}

static void test_retry_after_timeout_succeeds() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.retry = {10, 1};
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    run(machine, 0, 55);  // Timeout nach 50 ms -> RetryDelay
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::PipelineState::RetryDelay),
                            static_cast<std::uint8_t>(machine.state()));

    fixture.source.seed(2.0F, 60);  // Daten treffen während des Retries ein
    run(machine, 56, 80);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT32(0, machine.failedCycles());
}

static void test_retry_limit_reached_fails_cycle_and_continues() {
    Fixture fixture;
    fixture.wire();
    auto config = baseConfig();
    config.retry = {10, 2};
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, config);
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    run(machine, 0, 250);  // nie Daten: Initialversuch + 2 Retries scheitern
    TEST_ASSERT_EQUAL_UINT32(1, machine.failedCycles());
    ASSERT_CODE(Timeout, machine.lastStatus());

    // Pipeline lebt weiter.
    fixture.source.seed(1.0F, 300);
    run(machine, 251, 400);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
}

// ------------------------------------------------------------ Fault

static void test_permanent_error_enters_fault() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.sink1.failCount = 1;
    fixture.sink1.failStatus = mea::makeStatus(mea::StatusCode::InternalError, kSink1);
    fixture.source.seed(3.0F, 0);
    run(machine, 0, 10);

    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::PipelineState::Fault),
                            static_cast<std::uint8_t>(machine.state()));
    ASSERT_CODE(InternalError, machine.update(11));  // sticky
    ASSERT_CODE(InternalError, machine.enable(12));  // enable() hilft nicht

    // Nur ein explizites begin() verlässt Fault.
    TEST_ASSERT_TRUE(machine.begin(20).ok());
    fixture.source.seed(1.0F, 20);
    run(machine, 20, 30);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
}

// ------------------------------------------------------------ Zeit & Lebenszyklus

static void test_timestamp_rollover_is_handled() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());

    const mea::TimestampMs nearMax = std::numeric_limits<std::uint32_t>::max() - 3U;
    TEST_ASSERT_TRUE(machine.begin(nearMax).ok());
    fixture.source.seed(3.0F, nearMax);

    // Der Zyklus läuft über den uint32-Überlauf hinweg (nearMax .. nearMax+10).
    for (mea::TimestampMs offset = 0; offset <= 10; ++offset) {
        (void)machine.update(static_cast<mea::TimestampMs>(nearMax + offset));
    }
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());

    // Auch das Zyklusintervall funktioniert über den Überlauf hinweg.
    fixture.source.seed(4.0F, 90);
    for (mea::TimestampMs offset = 11; offset <= 120; ++offset) {
        (void)machine.update(static_cast<mea::TimestampMs>(nearMax + offset));
    }
    TEST_ASSERT_EQUAL_UINT32(2, machine.completedCycles());
}

static void test_disable_and_reenable() {
    Fixture fixture;
    fixture.wire();
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.source.seed(1.0F, 0);
    run(machine, 0, 5);
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());

    TEST_ASSERT_TRUE(machine.disable().ok());
    fixture.source.seed(2.0F, 200);
    run(machine, 100, 300);  // deaktiviert: nichts passiert
    TEST_ASSERT_EQUAL_UINT32(1, machine.completedCycles());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::PipelineState::Disabled),
                            static_cast<std::uint8_t>(machine.state()));

    TEST_ASSERT_TRUE(machine.enable(301).ok());
    run(machine, 301, 310);
    TEST_ASSERT_EQUAL_UINT32(2, machine.completedCycles());
}

static void test_machine_never_reinitializes_components() {
    Fixture fixture;
    fixture.wire();  // beginAll() der Manager: je genau 1 begin()
    mea::MeasurementPipelineMachine machine(fixture.sources, fixture.processors,
                                            fixture.sinks, baseConfig());
    TEST_ASSERT_TRUE(machine.begin(0).ok());

    fixture.source.seed(1.0F, 0);
    run(machine, 0, 10);
    TEST_ASSERT_TRUE(machine.begin(20).ok());  // erneutes begin() der Maschine
    fixture.source.seed(2.0F, 20);
    run(machine, 20, 30);

    TEST_ASSERT_EQUAL_UINT32(1, fixture.source.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.processor1.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.sink1.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(2, machine.completedCycles());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_update_before_begin_reports_not_initialized);
    RUN_TEST(test_invalid_configuration_is_rejected);
    RUN_TEST(test_duplicate_ids_are_rejected);
    RUN_TEST(test_missing_component_id_is_reported);
    RUN_TEST(test_happy_path_completes_cycle);
    RUN_TEST(test_start_not_immediately_waits_for_enable);
    RUN_TEST(test_source_busy_keeps_waiting);
    RUN_TEST(test_source_no_data_race_keeps_waiting);
    RUN_TEST(test_acquisition_timeout_fails_cycle);
    RUN_TEST(test_processor_error_fails_cycle);
    RUN_TEST(test_multiple_processors_run_in_order);
    RUN_TEST(test_multiple_sinks_all_receive);
    RUN_TEST(test_sink_would_block_enters_backpressure_and_recovers);
    RUN_TEST(test_publish_timeout_drops_measurement);
    RUN_TEST(test_temporary_sink_error_is_retried_successfully);
    RUN_TEST(test_retry_after_timeout_succeeds);
    RUN_TEST(test_retry_limit_reached_fails_cycle_and_continues);
    RUN_TEST(test_permanent_error_enters_fault);
    RUN_TEST(test_timestamp_rollover_is_handled);
    RUN_TEST(test_disable_and_reenable);
    RUN_TEST(test_machine_never_reinitializes_components);
    return UNITY_END();
}
