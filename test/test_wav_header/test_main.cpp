#include <string.h>
#include <unity.h>

#include "WavHeader.h"

void setUp(void) {}
void tearDown(void) {}

// Two blocks of data: 512 bytes, 1010 samples.
// RIFF size = 60 - 8 + 512 = 564 = 0x234.
// Byte rate = 8000 * 256 / 505 = 4055 (0x0FD7), rounded down.
void test_header_bytes_for_two_blocks(void) {
    uint8_t h[wav::kHeaderBytes];
    wav::writeHeader(h, 512);

    const uint8_t want[wav::kHeaderBytes] = {
        'R', 'I', 'F', 'F', 0x34, 0x02, 0x00, 0x00, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 20, 0, 0, 0,
        0x11, 0x00,             // IMA ADPCM
        0x01, 0x00,             // mono
        0x40, 0x1F, 0x00, 0x00, // 8000 Hz
        0xD7, 0x0F, 0x00, 0x00, // 4055 bytes/s
        0x00, 0x01,             // block align 256
        0x04, 0x00,             // 4 bits
        0x02, 0x00,             // cbSize
        0xF9, 0x01,             // 505 samples per block
        'f', 'a', 'c', 't', 4, 0, 0, 0,
        0xF2, 0x03, 0x00, 0x00, // 1010 samples
        'd', 'a', 't', 'a', 0x00, 0x02, 0x00, 0x00};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, h, wav::kHeaderBytes);
}

void test_data_size_reads_back(void) {
    uint8_t h[wav::kHeaderBytes];
    wav::writeHeader(h, 256 * 1234);
    uint32_t dataBytes = 0;
    TEST_ASSERT_TRUE(wav::readDataBytes(h, &dataBytes));
    TEST_ASSERT_EQUAL_UINT32(256 * 1234, dataBytes);
}

// The header written when a recording starts says 0 bytes of data.
void test_placeholder_header_reads_as_empty(void) {
    uint8_t h[wav::kHeaderBytes];
    wav::writeHeader(h, 0);
    uint32_t dataBytes = 99;
    TEST_ASSERT_TRUE(wav::readDataBytes(h, &dataBytes));
    TEST_ASSERT_EQUAL_UINT32(0, dataBytes);
}

void test_foreign_bytes_are_not_a_header(void) {
    uint8_t h[wav::kHeaderBytes] = {};
    uint32_t dataBytes = 0;
    TEST_ASSERT_FALSE(wav::readDataBytes(h, &dataBytes));

    wav::writeHeader(h, 256);
    h[20] = 0x01; // PCM instead of ADPCM
    TEST_ASSERT_FALSE(wav::readDataBytes(h, &dataBytes));
}

void test_repair_keeps_whole_blocks_only(void) {
    TEST_ASSERT_EQUAL_UINT32(0, wav::repairedDataBytes(0));
    TEST_ASSERT_EQUAL_UINT32(0, wav::repairedDataBytes(59));
    TEST_ASSERT_EQUAL_UINT32(0, wav::repairedDataBytes(60));
    TEST_ASSERT_EQUAL_UINT32(0, wav::repairedDataBytes(60 + 255));
    TEST_ASSERT_EQUAL_UINT32(256, wav::repairedDataBytes(60 + 256));
    TEST_ASSERT_EQUAL_UINT32(768, wav::repairedDataBytes(60 + 3 * 256 + 100));
}

// 16 blocks = 8080 samples = 1.01 s; 15 blocks = 7575 samples = 0.95 s.
void test_duration_is_rounded_down_to_seconds(void) {
    TEST_ASSERT_EQUAL_UINT32(1010, wav::samplesForDataBytes(512));
    TEST_ASSERT_EQUAL_UINT32(1, wav::secondsForDataBytes(16 * 256));
    TEST_ASSERT_EQUAL_UINT32(0, wav::secondsForDataBytes(15 * 256));
}

// 6 000 000 bytes = 23437 whole blocks = 11 835 685 samples = 1479.46 s.
void test_time_left_for_free_space(void) {
    TEST_ASSERT_EQUAL_UINT32(1479, wav::secondsForFreeBytes(6000000));
    TEST_ASSERT_EQUAL_UINT32(0, wav::secondsForFreeBytes(255));
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_header_bytes_for_two_blocks);
    RUN_TEST(test_data_size_reads_back);
    RUN_TEST(test_placeholder_header_reads_as_empty);
    RUN_TEST(test_foreign_bytes_are_not_a_header);
    RUN_TEST(test_repair_keeps_whole_blocks_only);
    RUN_TEST(test_duration_is_rounded_down_to_seconds);
    RUN_TEST(test_time_left_for_free_space);

    return UNITY_END();
}
