#include <unity.h>

#include <cstddef>
#include <cstdint>

#include <MeaManagers.h>

void setUp() {}
void tearDown() {}

namespace {

/// Konfigurierbare Stub-Quelle für Manager-Tests.
class StubSource final : public mea::IMeasurementSource {
public:
    explicit StubSource(const mea::ComponentId id) noexcept : id_(id) {}

    [[nodiscard]] mea::ComponentId id() const noexcept override { return id_; }

    mea::Status begin() noexcept override {
        ++beginCalls;
        return beginResult;
    }

    mea::Status update(mea::TimestampMs) noexcept override {
        ++updateCalls;
        return updateResult;
    }

    [[nodiscard]] std::size_t available() const noexcept override { return 0; }

    mea::Status read(mea::Measurement&) noexcept override {
        return mea::makeStatus(mea::StatusCode::NoData, id_);
    }

    mea::Status beginResult{mea::okStatus()};
    mea::Status updateResult{mea::okStatus()};
    std::uint32_t beginCalls{0};
    std::uint32_t updateCalls{0};

private:
    mea::ComponentId id_;
};

}  // namespace

// ---------------------------------------------------------------- FixedRegistry

static void test_registry_rejects_invalid_id() {
    StubSource invalid(mea::InvalidComponentId);
    mea::FixedRegistry<mea::IMeasurementSource, 2> registry;
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::InvalidArgument),
                            static_cast<std::uint8_t>(registry.add(invalid).code));
    TEST_ASSERT_EQUAL_size_t(0, registry.size());
}

static void test_registry_rejects_duplicate_id() {
    StubSource first(7);
    StubSource duplicate(7);
    mea::FixedRegistry<mea::IMeasurementSource, 2> registry;
    TEST_ASSERT_TRUE(registry.add(first).ok());
    const mea::Status status = registry.add(duplicate);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::DuplicateId),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(7, status.origin);
}

static void test_registry_reports_capacity() {
    StubSource one(1);
    StubSource two(2);
    mea::FixedRegistry<mea::IMeasurementSource, 1> registry;
    TEST_ASSERT_TRUE(registry.add(one).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::CapacityExceeded),
                            static_cast<std::uint8_t>(registry.add(two).code));
    TEST_ASSERT_EQUAL_size_t(1, registry.size());
    TEST_ASSERT_EQUAL_size_t(1, registry.capacity());
}

static void test_registry_find_and_iteration() {
    StubSource one(1);
    StubSource two(2);
    mea::FixedRegistry<mea::IMeasurementSource, 4> registry;
    TEST_ASSERT_TRUE(registry.add(one).ok());
    TEST_ASSERT_TRUE(registry.add(two).ok());

    TEST_ASSERT_EQUAL_PTR(&one, registry.find(1));
    TEST_ASSERT_EQUAL_PTR(&two, registry.find(2));
    TEST_ASSERT_NULL(registry.find(3));

    // Iteration in Registrierungsreihenfolge, deterministisch.
    TEST_ASSERT_EQUAL_PTR(&one, registry.at(0));
    TEST_ASSERT_EQUAL_PTR(&two, registry.at(1));
    TEST_ASSERT_NULL(registry.at(2));
}

// ---------------------------------------------------------------- Lebenszyklus

static void test_begin_all_is_once_only() {
    StubSource source(1);
    mea::SensorManager<2> manager;
    TEST_ASSERT_TRUE(manager.registerComponent(source).ok());

    TEST_ASSERT_TRUE(manager.beginAll().ok());
    TEST_ASSERT_EQUAL_UINT32(1, source.beginCalls);

    const mea::Status second = manager.beginAll();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::AlreadyInitialized),
        static_cast<std::uint8_t>(second.code));
    TEST_ASSERT_EQUAL_UINT32(1, source.beginCalls);  // keine Neuinitialisierung
}

static void test_begin_all_failure_carries_origin_and_is_retryable() {
    StubSource good(1);
    StubSource bad(2);
    bad.beginResult = mea::makeStatus(mea::StatusCode::IoError, mea::InvalidComponentId);

    mea::SensorManager<2> manager;
    TEST_ASSERT_TRUE(manager.registerComponent(good).ok());
    TEST_ASSERT_TRUE(manager.registerComponent(bad).ok());

    const mea::Status status = manager.beginAll();
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::IoError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(2, status.origin);    // Manager trägt Verursacher nach
    TEST_ASSERT_EQUAL_UINT32(1, good.beginCalls);  // alle Komponenten versucht
    TEST_ASSERT_FALSE(manager.initialized());

    // Nach Behebung darf beginAll() wiederholt werden.
    bad.beginResult = mea::okStatus();
    TEST_ASSERT_TRUE(manager.beginAll().ok());
    TEST_ASSERT_TRUE(manager.initialized());
}

static void test_register_after_begin_is_rejected() {
    StubSource source(1);
    StubSource late(2);
    mea::SensorManager<2> manager;
    TEST_ASSERT_TRUE(manager.registerComponent(source).ok());
    TEST_ASSERT_TRUE(manager.beginAll().ok());

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::AlreadyInitialized),
        static_cast<std::uint8_t>(manager.registerComponent(late).code));
}

// ---------------------------------------------------------------- updateAll

static void test_update_all_requires_begin() {
    mea::SensorManager<1> manager;
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::NotInitialized),
                            static_cast<std::uint8_t>(manager.updateAll(0).code));
}

static void test_update_all_does_not_stop_on_component_error() {
    StubSource failing(1);
    StubSource healthy(2);
    failing.updateResult = mea::makeStatus(mea::StatusCode::IoError, 1);

    mea::SensorManager<2> manager;
    TEST_ASSERT_TRUE(manager.registerComponent(failing).ok());
    TEST_ASSERT_TRUE(manager.registerComponent(healthy).ok());
    TEST_ASSERT_TRUE(manager.beginAll().ok());

    const mea::Status status = manager.updateAll(100);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::IoError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(1, status.origin);
    TEST_ASSERT_EQUAL_UINT32(1, healthy.updateCalls);  // zweite Komponente lief trotzdem
}

static void test_update_all_treats_transient_as_success() {
    StubSource busy(1);
    busy.updateResult = mea::makeStatus(mea::StatusCode::Busy, 1);

    mea::SensorManager<1> manager;
    TEST_ASSERT_TRUE(manager.registerComponent(busy).ok());
    TEST_ASSERT_TRUE(manager.beginAll().ok());
    TEST_ASSERT_TRUE(manager.updateAll(100).ok());
}

// ---------------------------------------------------------------- Health

static void test_health_counters() {
    StubSource source(5);
    mea::SensorManager<1> manager;
    TEST_ASSERT_TRUE(manager.registerComponent(source).ok());
    TEST_ASSERT_TRUE(manager.beginAll().ok());

    TEST_ASSERT_TRUE(manager.updateAll(10).ok());
    source.updateResult = mea::makeStatus(mea::StatusCode::IoError, 5, 99);
    TEST_ASSERT_FALSE(manager.updateAll(20).ok());

    const mea::ComponentHealth health = manager.health(5);
    TEST_ASSERT_EQUAL_UINT16(5, health.componentId);
    TEST_ASSERT_EQUAL_UINT32(2, health.successCount);  // begin() + 1 update
    TEST_ASSERT_EQUAL_UINT32(1, health.errorCount);
    TEST_ASSERT_EQUAL_UINT32(10, health.lastSuccessMs);
    TEST_ASSERT_EQUAL_UINT16(99, health.lastStatus.detail);
}

static void test_health_unknown_id_reports_not_found() {
    mea::SensorManager<1> manager;
    const mea::ComponentHealth health = manager.health(42);
    TEST_ASSERT_EQUAL_UINT16(mea::InvalidComponentId, health.componentId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::NotFound),
                            static_cast<std::uint8_t>(health.lastStatus.code));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_registry_rejects_invalid_id);
    RUN_TEST(test_registry_rejects_duplicate_id);
    RUN_TEST(test_registry_reports_capacity);
    RUN_TEST(test_registry_find_and_iteration);
    RUN_TEST(test_begin_all_is_once_only);
    RUN_TEST(test_begin_all_failure_carries_origin_and_is_retryable);
    RUN_TEST(test_register_after_begin_is_rejected);
    RUN_TEST(test_update_all_requires_begin);
    RUN_TEST(test_update_all_does_not_stop_on_component_error);
    RUN_TEST(test_update_all_treats_transient_as_success);
    RUN_TEST(test_health_counters);
    RUN_TEST(test_health_unknown_id_reports_not_found);
    return UNITY_END();
}
