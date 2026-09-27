#include <string.h>
#include <unity.h>

#include "LevelMeter.h"

void setUp(void) {}
void tearDown(void) {}

// 0 dBFS is full height, -60 dBFS and below is nothing, -30 dBFS is half.
void test_level_scale(void) {
    TEST_ASSERT_EQUAL_UINT8(0, LevelMeter::levelForPeak(0));
    TEST_ASSERT_EQUAL_UINT8(0, LevelMeter::levelForPeak(32));     // -60.2 dBFS
    TEST_ASSERT_EQUAL_UINT8(255, LevelMeter::levelForPeak(32767)); // 0 dBFS
    TEST_ASSERT_EQUAL_UINT8(255, LevelMeter::levelForPeak(40000)); // clipped input
    // 32768 * 10^(-30/20) = 1036.2
    TEST_ASSERT_UINT8_WITHIN(1, 127, LevelMeter::levelForPeak(1036));
}

void test_a_new_meter_is_flat(void) {
    LevelMeter m;
    for (size_t i = 0; i < LevelMeter::kBars; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, m.bar(i));
    }
}

void test_the_newest_bar_is_on_the_right(void) {
    LevelMeter m;
    const int16_t loud[3] = {100, -32767, 5};
    const int16_t silent[3] = {0, 0, 0};
    m.push(loud, 3); // negative peaks count too
    m.push(silent, 3);

    TEST_ASSERT_EQUAL_UINT8(0, m.bar(LevelMeter::kBars - 1));
    TEST_ASSERT_EQUAL_UINT8(255, m.bar(LevelMeter::kBars - 2));
    TEST_ASSERT_EQUAL_UINT8(0, m.bar(0));
}

void test_old_bars_scroll_out(void) {
    LevelMeter m;
    const int16_t loud[1] = {32767};
    const int16_t silent[1] = {0};
    m.push(loud, 1);
    for (size_t i = 0; i < LevelMeter::kBars - 1; ++i) {
        m.push(silent, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(255, m.bar(0)); // about to leave
    m.push(silent, 1);
    TEST_ASSERT_EQUAL_UINT8(0, m.bar(0));
}

void test_clear_flattens_and_bumps_the_version(void) {
    LevelMeter m;
    const int16_t loud[1] = {32767};
    m.push(loud, 1);
    const uint32_t before = m.version();
    m.clear();
    TEST_ASSERT_EQUAL_UINT8(0, m.bar(LevelMeter::kBars - 1));
    TEST_ASSERT_TRUE(m.version() != before);
}

void test_hex_lists_the_bars_oldest_first(void) {
    LevelMeter m;
    const int16_t loud[1] = {32767};
    m.push(loud, 1); // newest bar: 255
    char hex[2 * LevelMeter::kBars + 1];
    m.toHex(hex);
    TEST_ASSERT_EQUAL_UINT32(2 * LevelMeter::kBars, strlen(hex));
    TEST_ASSERT_EQUAL_STRING_LEN("0000", hex, 4);
    TEST_ASSERT_EQUAL_STRING("ff", hex + 2 * (LevelMeter::kBars - 1));
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_level_scale);
    RUN_TEST(test_a_new_meter_is_flat);
    RUN_TEST(test_the_newest_bar_is_on_the_right);
    RUN_TEST(test_old_bars_scroll_out);
    RUN_TEST(test_clear_flattens_and_bumps_the_version);
    RUN_TEST(test_hex_lists_the_bars_oldest_first);

    return UNITY_END();
}
