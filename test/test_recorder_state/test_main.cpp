#include <unity.h>

#include "RecorderState.h"

void setUp(void) {}
void tearDown(void) {}

static const bool kRoom = true;
static const bool kFull = false;

static RecorderState started() {
    RecorderState s;
    s.begin(0);
    return s;
}

// Update helpers: which key goes down in this tick.
static Command none(RecorderState &s, uint32_t t, bool room = kRoom) {
    return s.update(t, false, false, room);
}
static Command key1(RecorderState &s, uint32_t t, bool room = kRoom) {
    return s.update(t, true, false, room);
}
static Command key2(RecorderState &s, uint32_t t, bool room = kRoom) {
    return s.update(t, false, true, room);
}

void test_starts_ready(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
    TEST_ASSERT_EQUAL(Command::None, none(s, 100000));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

void test_key1_starts_and_stops_a_recording(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Command::StartRecording, key1(s, 1000));
    TEST_ASSERT_EQUAL(Mode::Recording, s.mode());

    TEST_ASSERT_EQUAL(Command::StopRecording, key1(s, 5000));
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    TEST_ASSERT_FALSE(s.stoppedFull());
}

void test_saved_turns_into_ready_after_four_seconds(void) {
    RecorderState s = started();
    key1(s, 1000);
    key1(s, 5000); // Saved from 5000
    TEST_ASSERT_EQUAL(Command::None, none(s, 8999));
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    TEST_ASSERT_EQUAL(Command::None, none(s, 9000));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
    TEST_ASSERT_EQUAL(Mode::Saved, s.previous());
}

// Screen 3 shows no hints: a key only cuts the summary short.
void test_a_key_on_saved_only_goes_back_to_ready(void) {
    RecorderState s = started();
    key1(s, 1000);
    key1(s, 5000);
    TEST_ASSERT_EQUAL(Command::None, key1(s, 6000));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());

    key1(s, 7000);
    key1(s, 8000);
    TEST_ASSERT_EQUAL(Command::None, key2(s, 9000));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

void test_running_out_of_memory_stops_the_recording(void) {
    RecorderState s = started();
    key1(s, 1000);
    TEST_ASSERT_EQUAL(Command::StopRecording, none(s, 2000, kFull));
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    TEST_ASSERT_TRUE(s.stoppedFull());
}

// Screen 5: nothing to record into.
void test_key1_does_nothing_when_memory_is_full(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Command::None, key1(s, 1000, kFull));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

void test_key2_toggles_wifi(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Command::WifiOn, key2(s, 1000));
    TEST_ASSERT_EQUAL(Mode::Wifi, s.mode());
    TEST_ASSERT_EQUAL(Command::WifiOff, key2(s, 2000));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

void test_wifi_works_with_full_memory_too(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Command::WifiOn, key2(s, 1000, kFull));
    TEST_ASSERT_EQUAL(Command::WifiOff, key2(s, 2000, kFull));
}

void test_key2_is_ignored_while_recording(void) {
    RecorderState s = started();
    key1(s, 1000);
    TEST_ASSERT_EQUAL(Command::None, key2(s, 2000));
    TEST_ASSERT_EQUAL(Mode::Recording, s.mode());
}

void test_key1_is_ignored_in_wifi_mode(void) {
    RecorderState s = started();
    key2(s, 1000);
    TEST_ASSERT_EQUAL(Command::None, key1(s, 2000));
    TEST_ASSERT_EQUAL(Mode::Wifi, s.mode());
}

void test_a_failed_start_goes_back_to_ready(void) {
    RecorderState s = started();
    key1(s, 1000);
    s.cancelRecording(1001);
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

void test_only_saved_to_ready_and_wifi_changes_fade(void) {
    TEST_ASSERT_TRUE(RecorderState::fades(Mode::Saved, Mode::Ready));
    TEST_ASSERT_TRUE(RecorderState::fades(Mode::Ready, Mode::Wifi));
    TEST_ASSERT_TRUE(RecorderState::fades(Mode::Wifi, Mode::Ready));
    TEST_ASSERT_FALSE(RecorderState::fades(Mode::Ready, Mode::Recording));
    TEST_ASSERT_FALSE(RecorderState::fades(Mode::Recording, Mode::Saved));
}

// 300 ms of the old screen going dark, then 300 ms of the new one coming
// up. Levels are 255 * remaining / 300, rounded down.
void test_fade_goes_to_black_and_back(void) {
    RecorderState s = started();
    key1(s, 1000);
    key1(s, 2000);
    none(s, 6000); // Saved -> Ready at 6000

    Fade f = s.fade(6000);
    TEST_ASSERT_TRUE(f.showPrevious);
    TEST_ASSERT_EQUAL_UINT8(255, f.level);

    f = s.fade(6150);
    TEST_ASSERT_TRUE(f.showPrevious);
    TEST_ASSERT_EQUAL_UINT8(127, f.level);

    f = s.fade(6300);
    TEST_ASSERT_FALSE(f.showPrevious);
    TEST_ASSERT_EQUAL_UINT8(0, f.level);

    f = s.fade(6450);
    TEST_ASSERT_FALSE(f.showPrevious);
    TEST_ASSERT_EQUAL_UINT8(127, f.level);

    f = s.fade(6600);
    TEST_ASSERT_FALSE(f.showPrevious);
    TEST_ASSERT_EQUAL_UINT8(255, f.level);
}

void test_starting_a_recording_does_not_fade(void) {
    RecorderState s = started();
    key1(s, 1000);
    const Fade f = s.fade(1000);
    TEST_ASSERT_FALSE(f.showPrevious);
    TEST_ASSERT_EQUAL_UINT8(255, f.level);
}

void test_saved_timeout_survives_the_millis_rollover(void) {
    RecorderState s;
    const uint32_t nearOverflow = 0xFFFFF000u; // 4096 ms before the rollover
    s.begin(nearOverflow);
    key1(s, nearOverflow);
    key1(s, nearOverflow + 100); // Saved
    none(s, nearOverflow + 100 + 3999);
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    none(s, nearOverflow + 100 + 4000); // already past zero
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_starts_ready);
    RUN_TEST(test_key1_starts_and_stops_a_recording);
    RUN_TEST(test_saved_turns_into_ready_after_four_seconds);
    RUN_TEST(test_a_key_on_saved_only_goes_back_to_ready);
    RUN_TEST(test_running_out_of_memory_stops_the_recording);
    RUN_TEST(test_key1_does_nothing_when_memory_is_full);
    RUN_TEST(test_key2_toggles_wifi);
    RUN_TEST(test_wifi_works_with_full_memory_too);
    RUN_TEST(test_key2_is_ignored_while_recording);
    RUN_TEST(test_key1_is_ignored_in_wifi_mode);
    RUN_TEST(test_a_failed_start_goes_back_to_ready);
    RUN_TEST(test_only_saved_to_ready_and_wifi_changes_fade);
    RUN_TEST(test_fade_goes_to_black_and_back);
    RUN_TEST(test_starting_a_recording_does_not_fade);
    RUN_TEST(test_saved_timeout_survives_the_millis_rollover);

    return UNITY_END();
}
