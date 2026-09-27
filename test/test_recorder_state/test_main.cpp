#include <unity.h>

#include "RecorderState.h"

void setUp(void) {}
void tearDown(void) {}

static RecorderState started() {
    RecorderState s;
    s.begin(0);
    return s;
}

// One tick with a single event.
enum class Ev { None, Key1, Key2, WebRecord, WebStop };

static Command tick(RecorderState &s, uint32_t t, Ev ev = Ev::None, bool room = true) {
    Input in;
    in.key1 = ev == Ev::Key1;
    in.key2 = ev == Ev::Key2;
    in.webRecord = ev == Ev::WebRecord;
    in.webStop = ev == Ev::WebStop;
    in.roomLeft = room;
    return s.update(t, in);
}

static const bool kFull = false;

// ---- without Wi-Fi (screens 1, 2, 3, 5) -------------------------------------

void test_starts_ready_without_wifi(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
    TEST_ASSERT_FALSE(s.wifi());
    TEST_ASSERT_EQUAL(Command::None, tick(s, 100000));
}

void test_key1_starts_and_stops_a_recording(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Command::StartRecording, tick(s, 1000, Ev::Key1));
    TEST_ASSERT_EQUAL(Mode::Recording, s.mode());
    TEST_ASSERT_EQUAL(Command::StopRecording, tick(s, 5000, Ev::Key1));
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    TEST_ASSERT_FALSE(s.stoppedFull());
}

void test_saved_turns_into_ready_after_four_seconds(void) {
    RecorderState s = started();
    tick(s, 1000, Ev::Key1);
    tick(s, 5000, Ev::Key1); // Saved from 5000
    tick(s, 8999);
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    tick(s, 9000);
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
    TEST_ASSERT_EQUAL(Mode::Saved, s.previous().mode);
}

// Screen 3 shows no hints: a key only cuts the summary short.
void test_a_key_on_saved_only_goes_back_to_ready(void) {
    RecorderState s = started();
    tick(s, 1000, Ev::Key1);
    tick(s, 5000, Ev::Key1);
    TEST_ASSERT_EQUAL(Command::None, tick(s, 6000, Ev::Key1));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());

    tick(s, 7000, Ev::Key1);
    tick(s, 8000, Ev::Key1);
    TEST_ASSERT_EQUAL(Command::None, tick(s, 9000, Ev::Key2));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
    TEST_ASSERT_FALSE(s.wifi());
}

void test_running_out_of_memory_stops_the_recording(void) {
    RecorderState s = started();
    tick(s, 1000, Ev::Key1);
    TEST_ASSERT_EQUAL(Command::StopRecording, tick(s, 2000, Ev::None, kFull));
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    TEST_ASSERT_TRUE(s.stoppedFull());
}

// Screen 5: nothing to record into.
void test_key1_does_nothing_when_memory_is_full(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Command::None, tick(s, 1000, Ev::Key1, kFull));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

void test_key2_is_ignored_while_recording(void) {
    RecorderState s = started();
    tick(s, 1000, Ev::Key1);
    TEST_ASSERT_EQUAL(Command::None, tick(s, 2000, Ev::Key2));
    TEST_ASSERT_EQUAL(Mode::Recording, s.mode());
    TEST_ASSERT_FALSE(s.wifi());
}

void test_a_failed_start_goes_back_to_ready(void) {
    RecorderState s = started();
    tick(s, 1000, Ev::Key1);
    s.cancelRecording(1001);
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

// ---- Wi-Fi (screens 4, 8, 3, 9) ---------------------------------------------

static RecorderState withWifi() {
    RecorderState s = started();
    tick(s, 1000, Ev::Key2);
    return s;
}

void test_key2_toggles_wifi(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Command::WifiOn, tick(s, 1000, Ev::Key2));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
    TEST_ASSERT_TRUE(s.wifi());
    TEST_ASSERT_EQUAL(Command::WifiOff, tick(s, 2000, Ev::Key2));
    TEST_ASSERT_FALSE(s.wifi());
}

void test_wifi_works_with_full_memory_too(void) {
    RecorderState s = started();
    TEST_ASSERT_EQUAL(Command::WifiOn, tick(s, 1000, Ev::Key2, kFull));
    TEST_ASSERT_EQUAL(Command::WifiOff, tick(s, 2000, Ev::Key2, kFull));
}

// Screen 4 shows no KEY1 hint: in Wi-Fi mode recording starts from the web.
void test_key1_does_nothing_on_the_wifi_screen(void) {
    RecorderState s = withWifi();
    TEST_ASSERT_EQUAL(Command::None, tick(s, 2000, Ev::Key1));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

void test_the_web_page_starts_and_stops_a_recording(void) {
    RecorderState s = withWifi();
    TEST_ASSERT_EQUAL(Command::StartRecording, tick(s, 2000, Ev::WebRecord));
    TEST_ASSERT_EQUAL(Mode::Recording, s.mode());
    TEST_ASSERT_TRUE(s.wifi()); // the access point stays on: screen 8
    TEST_ASSERT_EQUAL(Command::StopRecording, tick(s, 5000, Ev::WebStop));
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    TEST_ASSERT_TRUE(s.wifi());
    TEST_ASSERT_FALSE(s.stoppedFull());
}

// Screen 8 shows the same Stop hint as screen 2.
void test_key1_stops_a_recording_started_from_the_web(void) {
    RecorderState s = withWifi();
    tick(s, 2000, Ev::WebRecord);
    TEST_ASSERT_EQUAL(Command::StopRecording, tick(s, 3000, Ev::Key1));
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
}

void test_key2_and_web_record_are_ignored_while_recording_over_wifi(void) {
    RecorderState s = withWifi();
    tick(s, 2000, Ev::WebRecord);
    TEST_ASSERT_EQUAL(Command::None, tick(s, 3000, Ev::Key2));
    TEST_ASSERT_EQUAL(Command::None, tick(s, 3001, Ev::WebRecord));
    TEST_ASSERT_EQUAL(Mode::Recording, s.mode());
    TEST_ASSERT_TRUE(s.wifi());
}

void test_saved_goes_back_to_the_wifi_screen(void) {
    RecorderState s = withWifi();
    tick(s, 2000, Ev::WebRecord);
    tick(s, 3000, Ev::WebStop);
    tick(s, 7000);
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
    TEST_ASSERT_TRUE(s.wifi());
}

// The page shows Record while the stick still shows the summary.
void test_web_record_on_saved_starts_the_next_recording(void) {
    RecorderState s = withWifi();
    tick(s, 2000, Ev::WebRecord);
    tick(s, 3000, Ev::WebStop);
    TEST_ASSERT_EQUAL(Command::StartRecording, tick(s, 4000, Ev::WebRecord));
    TEST_ASSERT_EQUAL(Mode::Recording, s.mode());
}

void test_web_record_does_nothing_when_memory_is_full(void) {
    RecorderState s = withWifi();
    TEST_ASSERT_EQUAL(Command::None, tick(s, 2000, Ev::WebRecord, kFull));
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

void test_memory_running_out_over_wifi_ends_on_the_wifi_screen(void) {
    RecorderState s = withWifi();
    tick(s, 2000, Ev::WebRecord);
    TEST_ASSERT_EQUAL(Command::StopRecording, tick(s, 3000, Ev::None, kFull));
    TEST_ASSERT_TRUE(s.stoppedFull());
    tick(s, 7000, Ev::None, kFull);
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode()); // screen 9
    TEST_ASSERT_TRUE(s.wifi());
}

// ---- fades ------------------------------------------------------------------

void test_which_view_changes_fade(void) {
    const View ready{Mode::Ready, false}, readyWifi{Mode::Ready, true};
    const View rec{Mode::Recording, false}, recWifi{Mode::Recording, true};
    const View saved{Mode::Saved, false}, savedWifi{Mode::Saved, true};
    TEST_ASSERT_TRUE(RecorderState::fades(saved, ready));          // 3 -> 1
    TEST_ASSERT_TRUE(RecorderState::fades(savedWifi, readyWifi));  // 3 -> 4
    TEST_ASSERT_TRUE(RecorderState::fades(ready, readyWifi));      // 1 -> 4
    TEST_ASSERT_TRUE(RecorderState::fades(readyWifi, ready));      // 4 -> 1
    TEST_ASSERT_FALSE(RecorderState::fades(ready, rec));           // 1 -> 2
    TEST_ASSERT_FALSE(RecorderState::fades(readyWifi, recWifi));   // 4 -> 8
    TEST_ASSERT_FALSE(RecorderState::fades(recWifi, savedWifi));   // 8 -> 3
    TEST_ASSERT_FALSE(RecorderState::fades(savedWifi, recWifi));   // 3 -> 8
}

// 300 ms of the old screen going dark, then 300 ms of the new one coming
// up. Levels are 255 * remaining / 300, rounded down.
void test_fade_goes_to_black_and_back(void) {
    RecorderState s = started();
    tick(s, 1000, Ev::Key1);
    tick(s, 2000, Ev::Key1);
    tick(s, 6000); // Saved -> Ready at 6000

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

void test_switching_wifi_on_fades(void) {
    RecorderState s = started();
    tick(s, 1000, Ev::Key2);
    const Fade f = s.fade(1100);
    TEST_ASSERT_TRUE(f.showPrevious);
    TEST_ASSERT_FALSE(s.previous().wifi);
}

void test_starting_a_recording_does_not_fade(void) {
    RecorderState s = withWifi();
    tick(s, 5000, Ev::WebRecord);
    const Fade f = s.fade(5000);
    TEST_ASSERT_FALSE(f.showPrevious);
    TEST_ASSERT_EQUAL_UINT8(255, f.level);
}

void test_saved_timeout_survives_the_millis_rollover(void) {
    RecorderState s;
    const uint32_t nearOverflow = 0xFFFFF000u; // 4096 ms before the rollover
    s.begin(nearOverflow);
    tick(s, nearOverflow, Ev::Key1);
    tick(s, nearOverflow + 100, Ev::Key1); // Saved
    tick(s, nearOverflow + 100 + 3999);
    TEST_ASSERT_EQUAL(Mode::Saved, s.mode());
    tick(s, nearOverflow + 100 + 4000); // already past zero
    TEST_ASSERT_EQUAL(Mode::Ready, s.mode());
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_starts_ready_without_wifi);
    RUN_TEST(test_key1_starts_and_stops_a_recording);
    RUN_TEST(test_saved_turns_into_ready_after_four_seconds);
    RUN_TEST(test_a_key_on_saved_only_goes_back_to_ready);
    RUN_TEST(test_running_out_of_memory_stops_the_recording);
    RUN_TEST(test_key1_does_nothing_when_memory_is_full);
    RUN_TEST(test_key2_is_ignored_while_recording);
    RUN_TEST(test_a_failed_start_goes_back_to_ready);

    RUN_TEST(test_key2_toggles_wifi);
    RUN_TEST(test_wifi_works_with_full_memory_too);
    RUN_TEST(test_key1_does_nothing_on_the_wifi_screen);
    RUN_TEST(test_the_web_page_starts_and_stops_a_recording);
    RUN_TEST(test_key1_stops_a_recording_started_from_the_web);
    RUN_TEST(test_key2_and_web_record_are_ignored_while_recording_over_wifi);
    RUN_TEST(test_saved_goes_back_to_the_wifi_screen);
    RUN_TEST(test_web_record_on_saved_starts_the_next_recording);
    RUN_TEST(test_web_record_does_nothing_when_memory_is_full);
    RUN_TEST(test_memory_running_out_over_wifi_ends_on_the_wifi_screen);

    RUN_TEST(test_which_view_changes_fade);
    RUN_TEST(test_fade_goes_to_black_and_back);
    RUN_TEST(test_switching_wifi_on_fades);
    RUN_TEST(test_starting_a_recording_does_not_fade);
    RUN_TEST(test_saved_timeout_survives_the_millis_rollover);

    return UNITY_END();
}
