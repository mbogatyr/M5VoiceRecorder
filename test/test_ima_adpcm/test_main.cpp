#include <math.h>
#include <string.h>
#include <unity.h>

#include "ImaAdpcm.h"

using ima::kBlockBytes;
using ima::kSamplesPerBlock;

void setUp(void) {}
void tearDown(void) {}

static void fillSine(int16_t *dst, size_t count, size_t offset, float hz, float amplitude) {
    for (size_t i = 0; i < count; ++i) {
        const float t = static_cast<float>(offset + i) / 8000.0f;
        dst[i] = static_cast<int16_t>(lrintf(amplitude * sinf(2.0f * 3.14159265f * hz * t)));
    }
}

void test_block_header_keeps_the_first_sample_and_the_step_index(void) {
    int16_t samples[kSamplesPerBlock] = {};
    samples[0] = -1234; // 0xFB2E
    uint8_t block[kBlockBytes];

    ima::Encoder encoder;
    encoder.encodeBlock(samples, block);

    TEST_ASSERT_EQUAL_HEX8(0x2E, block[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFB, block[1]);
    TEST_ASSERT_EQUAL_UINT8(0, block[2]); // a fresh encoder starts at index 0
    TEST_ASSERT_EQUAL_UINT8(0, block[3]);
}

// Worked out by hand with the IMA tables.
// Sample 1 = 100 from predictor 0, step 7: 100 >= 7, 93 >= 3, 90 >= 1, so
// code 7; the decoder adds 0 + 7 + 3 + 1 = 11, index goes to 8 (step 16).
// Sample 2 = 100 from 11, step 16: 89 >= 16, 73 >= 8, 65 >= 4, code 7 again;
// adds 2 + 16 + 8 + 4 = 30, giving 41.
void test_first_codes_match_a_hand_calculation(void) {
    int16_t samples[kSamplesPerBlock] = {};
    samples[1] = 100;
    samples[2] = 100;
    uint8_t block[kBlockBytes];
    ima::Encoder encoder;
    encoder.encodeBlock(samples, block);

    TEST_ASSERT_EQUAL_HEX8(0x77, block[4]); // both codes 7, earlier in the low nibble

    int16_t decoded[kSamplesPerBlock];
    TEST_ASSERT_TRUE(ima::decodeBlock(block, decoded));
    TEST_ASSERT_EQUAL_INT16(0, decoded[0]);
    TEST_ASSERT_EQUAL_INT16(11, decoded[1]);
    TEST_ASSERT_EQUAL_INT16(41, decoded[2]);
}

void test_silence_stays_exactly_silent(void) {
    int16_t samples[kSamplesPerBlock] = {};
    uint8_t block[kBlockBytes];
    ima::Encoder encoder;
    encoder.encodeBlock(samples, block);

    for (size_t i = 0; i < kBlockBytes; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, block[i]);
    }
    int16_t decoded[kSamplesPerBlock];
    TEST_ASSERT_TRUE(ima::decodeBlock(block, decoded));
    for (size_t i = 0; i < kSamplesPerBlock; ++i) {
        TEST_ASSERT_EQUAL_INT16(0, decoded[i]);
    }
}

// Speech-like material: a 440 Hz tone at -10 dBFS over four blocks. The
// first block is left out of the measurement because the step starts at
// its smallest and needs a few dozen samples to catch up.
void test_a_tone_survives_encoding_with_good_snr(void) {
    const size_t kBlocks = 4;
    int16_t input[kBlocks * kSamplesPerBlock];
    fillSine(input, kBlocks * kSamplesPerBlock, 0, 440.0f, 10000.0f);

    ima::Encoder encoder;
    double signal = 0, noise = 0;
    for (size_t b = 0; b < kBlocks; ++b) {
        uint8_t block[kBlockBytes];
        int16_t decoded[kSamplesPerBlock];
        encoder.encodeBlock(input + b * kSamplesPerBlock, block);
        TEST_ASSERT_TRUE(ima::decodeBlock(block, decoded));
        if (b == 0) {
            continue;
        }
        for (size_t i = 0; i < kSamplesPerBlock; ++i) {
            const double x = input[b * kSamplesPerBlock + i];
            const double e = x - decoded[i];
            signal += x * x;
            noise += e * e;
        }
    }
    const double snrDb = 10.0 * log10(signal / noise);
    TEST_ASSERT_TRUE_MESSAGE(snrDb > 25.0, "IMA ADPCM should give well over 25 dB on a tone");
}

// Full-scale square wave: the predictor must clamp, not wrap around.
void test_full_scale_input_does_not_overflow(void) {
    int16_t samples[kSamplesPerBlock];
    for (size_t i = 0; i < kSamplesPerBlock; ++i) {
        samples[i] = ((i / 40) % 2) ? -32768 : 32767;
    }
    ima::Encoder encoder;
    uint8_t block[kBlockBytes];
    int16_t decoded[kSamplesPerBlock];
    encoder.encodeBlock(samples, block); // warm up the step
    encoder.encodeBlock(samples, block);
    TEST_ASSERT_TRUE(ima::decodeBlock(block, decoded));

    // Near the end of each half-period the output has settled at the rail.
    TEST_ASSERT_TRUE(decoded[39] > 30000);
    TEST_ASSERT_TRUE(decoded[79] < -30000);
}

void test_step_index_carries_over_to_the_next_block(void) {
    int16_t samples[kSamplesPerBlock];
    fillSine(samples, kSamplesPerBlock, 0, 440.0f, 10000.0f);
    ima::Encoder encoder;
    uint8_t first[kBlockBytes], second[kBlockBytes];
    encoder.encodeBlock(samples, first);
    encoder.encodeBlock(samples, second);

    TEST_ASSERT_EQUAL_UINT8(0, first[2]);
    TEST_ASSERT_TRUE(second[2] > 30); // a loud tone needs large steps

    encoder.reset();
    encoder.encodeBlock(samples, first);
    TEST_ASSERT_EQUAL_UINT8(0, first[2]);
}

void test_decoder_rejects_a_step_index_out_of_range(void) {
    uint8_t block[kBlockBytes] = {};
    block[2] = 89;
    int16_t decoded[kSamplesPerBlock];
    TEST_ASSERT_FALSE(ima::decodeBlock(block, decoded));
}

void test_block_geometry(void) {
    TEST_ASSERT_EQUAL_UINT32(256, kBlockBytes);
    TEST_ASSERT_EQUAL_UINT32(505, kSamplesPerBlock);
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_block_header_keeps_the_first_sample_and_the_step_index);
    RUN_TEST(test_first_codes_match_a_hand_calculation);
    RUN_TEST(test_silence_stays_exactly_silent);
    RUN_TEST(test_a_tone_survives_encoding_with_good_snr);
    RUN_TEST(test_full_scale_input_does_not_overflow);
    RUN_TEST(test_step_index_carries_over_to_the_next_block);
    RUN_TEST(test_decoder_rejects_a_step_index_out_of_range);
    RUN_TEST(test_block_geometry);

    return UNITY_END();
}
