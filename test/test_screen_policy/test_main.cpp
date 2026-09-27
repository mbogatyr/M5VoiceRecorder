#include <unity.h>

#include "ScreenPolicy.h"

// By default the display turns off after 180000 ms without activity, and
// while recording it dims after 30000 ms instead.
static ScreenPolicy started() {
    ScreenPolicy policy;
    policy.begin(0);
    return policy;
}

void setUp(void) {}
void tearDown(void) {}

void test_display_is_on_right_after_start(void) {
    ScreenPolicy p = started();
    TEST_ASSERT_EQUAL(Backlight::On, p.update(0, false, false));
}

void test_display_stays_on_until_the_timeout_elapses(void) {
    ScreenPolicy p = started();
    TEST_ASSERT_EQUAL(Backlight::On, p.update(179999, false, false));
}

void test_display_turns_off_when_the_timeout_elapses(void) {
    ScreenPolicy p = started();
    TEST_ASSERT_EQUAL(Backlight::Off, p.update(180000, false, false));
}

void test_activity_wakes_a_sleeping_display(void) {
    ScreenPolicy p = started();
    p.update(180000, false, false);
    TEST_ASSERT_EQUAL(Backlight::On, p.update(180001, true, false));
}

void test_activity_restarts_the_countdown(void) {
    ScreenPolicy p = started();
    p.update(100000, true, false);
    TEST_ASSERT_EQUAL(Backlight::On, p.update(279999, false, false));
    TEST_ASSERT_EQUAL(Backlight::Off, p.update(280000, false, false));
}

void test_recording_dims_instead_of_turning_off(void) {
    ScreenPolicy p = started();
    TEST_ASSERT_EQUAL(Backlight::On, p.update(29999, false, true));
    TEST_ASSERT_EQUAL(Backlight::Dim, p.update(30000, false, true));
    TEST_ASSERT_EQUAL(Backlight::Dim, p.update(3600000, false, true));
}

void test_a_key_brightens_a_dimmed_recording_screen(void) {
    ScreenPolicy p = started();
    p.update(40000, false, true);
    TEST_ASSERT_EQUAL(Backlight::On, p.update(40001, true, true));
}

// A long recording ends with nobody touching the device: the idle time has
// long passed, so the display goes off as soon as recording stops.
void test_after_a_long_idle_recording_the_display_turns_off(void) {
    ScreenPolicy p = started();
    p.update(600000, false, true);
    TEST_ASSERT_EQUAL(Backlight::Off, p.update(600001, false, false));
}

void test_custom_timeouts_are_respected(void) {
    ScreenPolicy p(1000, 100);
    p.begin(0);
    TEST_ASSERT_EQUAL(Backlight::On, p.update(999, false, false));
    TEST_ASSERT_EQUAL(Backlight::Off, p.update(1000, false, false));
    TEST_ASSERT_EQUAL(Backlight::Dim, p.update(1000, false, true));
}

void test_timeout_survives_the_millis_rollover(void) {
    ScreenPolicy p;
    const uint32_t nearOverflow = 0xFFFFFF00u; // 256 ms before the rollover
    p.begin(nearOverflow);
    TEST_ASSERT_EQUAL(Backlight::On, p.update(nearOverflow + 100, false, false));
    TEST_ASSERT_EQUAL(Backlight::Off, p.update(nearOverflow + 180000, false, false));
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_display_is_on_right_after_start);
    RUN_TEST(test_display_stays_on_until_the_timeout_elapses);
    RUN_TEST(test_display_turns_off_when_the_timeout_elapses);
    RUN_TEST(test_activity_wakes_a_sleeping_display);
    RUN_TEST(test_activity_restarts_the_countdown);
    RUN_TEST(test_recording_dims_instead_of_turning_off);
    RUN_TEST(test_a_key_brightens_a_dimmed_recording_screen);
    RUN_TEST(test_after_a_long_idle_recording_the_display_turns_off);
    RUN_TEST(test_custom_timeouts_are_respected);
    RUN_TEST(test_timeout_survives_the_millis_rollover);

    return UNITY_END();
}
