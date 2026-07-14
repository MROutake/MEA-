#include <unity.h>

#include <cstdint>
#include <vector>

#include "FakeArduino74hc595.h"

void setUp() {}
void tearDown() {}

namespace {

static void assert_pattern(const std::array<uint8_t, 8>& actual,
                           const std::array<uint8_t, 8>& expected) {
    for (size_t i = 0; i < 8; ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], actual[i]);
    }
}


}  // namespace

static void test_commit_uses_setdata_like_sequence() {
    
    mea::ArduinoShiftWriterConfig config{};
    config.dataPin = 1;
    config.shiftClockPin = 2;
    config.storageClockPin = 3;
    config.enablePin = 4;
    config.masterResetPin = 5;
    config.bitOrder = 0;  // MSBFIRST

    mea::Arduino74hc595 writer(config);
    TEST_ASSERT_TRUE(writer.begin().ok());

    const std::uint8_t bytes[2] = {0xA5, 0x3C};
    TEST_ASSERT_TRUE(writer.setLatch(true).ok());
    TEST_ASSERT_TRUE(writer.write(bytes, 2).ok());

    const auto& calls = writer.shiftOutCalls();
    TEST_ASSERT_EQUAL_UINT32(2U, static_cast<std::uint32_t>(calls.size()));

    // Driver writes highest byte first (setData-like behavior).
    TEST_ASSERT_EQUAL_HEX8(0x3C, calls[0].value);
    TEST_ASSERT_EQUAL_HEX8(0xA5, calls[1].value);

    const std::array<uint8_t, 8> expected0{0, 0, 1, 1, 1, 1, 0, 0};
    const std::array<uint8_t, 8> expected1{1, 0, 1, 0, 0, 1, 0, 1};
    assert_pattern(calls[0].bitPattern, expected0);
    assert_pattern(calls[1].bitPattern, expected1);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_commit_uses_setdata_like_sequence);
    return UNITY_END();
}
