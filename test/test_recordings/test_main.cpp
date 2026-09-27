#include <unity.h>

#include "Recordings.h"

void setUp(void) {}
void tearDown(void) {}

void test_recording_names_are_recognized(void) {
    TEST_ASSERT_TRUE(rec::isRecordingName("REC_0001.wav"));
    TEST_ASSERT_TRUE(rec::isRecordingName("REC_9999.wav"));
}

// The web page passes names straight from the URL; nothing but a plain
// recording name may get through to the file system.
void test_anything_else_is_refused(void) {
    TEST_ASSERT_FALSE(rec::isRecordingName(nullptr));
    TEST_ASSERT_FALSE(rec::isRecordingName(""));
    TEST_ASSERT_FALSE(rec::isRecordingName("REC_0000.wav"));
    TEST_ASSERT_FALSE(rec::isRecordingName("rec_0001.wav"));
    TEST_ASSERT_FALSE(rec::isRecordingName("REC_001.wav"));
    TEST_ASSERT_FALSE(rec::isRecordingName("REC_00001.wav"));
    TEST_ASSERT_FALSE(rec::isRecordingName("REC_12a4.wav"));
    TEST_ASSERT_FALSE(rec::isRecordingName("REC_0001.WAV"));
    TEST_ASSERT_FALSE(rec::isRecordingName("REC_0001.wav.bak"));
    TEST_ASSERT_FALSE(rec::isRecordingName("/REC_0001.wav"));
    TEST_ASSERT_FALSE(rec::isRecordingName("../REC_001.wav"));
}

void test_number_round_trips_through_the_name(void) {
    char name[rec::kNameSize];
    rec::nameFor(7, name);
    TEST_ASSERT_EQUAL_STRING("REC_0007.wav", name);
    TEST_ASSERT_EQUAL_UINT32(7, rec::numberOf(name));
    TEST_ASSERT_EQUAL_UINT32(0, rec::numberOf("notes.txt"));
}

void test_numbering_starts_over_after_9999(void) {
    TEST_ASSERT_EQUAL_UINT32(1, rec::nextNumber(0));
    TEST_ASSERT_EQUAL_UINT32(42, rec::nextNumber(41));
    TEST_ASSERT_EQUAL_UINT32(1, rec::nextNumber(9999));
}

void test_durations(void) {
    char s[12];
    rec::formatDuration(0, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("00:00", s);
    rec::formatDuration(83, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("01:23", s);
    rec::formatDuration(3599, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("59:59", s);
    rec::formatDuration(3600, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("1:00:00", s);
    rec::formatDuration(3725, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("1:02:05", s);
}

void test_megabytes(void) {
    char s[16];
    rec::formatMegabytes(0, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("0.00 MB", s);
    rec::formatMegabytes(4999, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("0.00 MB", s);
    rec::formatMegabytes(5000, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("0.01 MB", s);
    rec::formatMegabytes(330000, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("0.33 MB", s);
    rec::formatMegabytes(5800000, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("5.80 MB", s);
    rec::formatMegabytes(12400000, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("12.4 MB", s);
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_recording_names_are_recognized);
    RUN_TEST(test_anything_else_is_refused);
    RUN_TEST(test_number_round_trips_through_the_name);
    RUN_TEST(test_numbering_starts_over_after_9999);
    RUN_TEST(test_durations);
    RUN_TEST(test_megabytes);

    return UNITY_END();
}
