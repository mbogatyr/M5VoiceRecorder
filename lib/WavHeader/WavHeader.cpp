#include "WavHeader.h"

#include <string.h>

namespace wav {

namespace {

constexpr uint16_t kFormatImaAdpcm = 0x0011;
constexpr uint16_t kChannels = 1;
constexpr uint16_t kBitsPerSample = 4;
constexpr uint32_t kByteRate =
    kSampleRate * ima::kBlockBytes / ima::kSamplesPerBlock;

void put16(uint8_t *p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}

void put32(uint8_t *p, uint32_t v) {
    put16(p, static_cast<uint16_t>(v));
    put16(p + 2, static_cast<uint16_t>(v >> 16));
}

uint16_t get16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t get32(const uint8_t *p) {
    return get16(p) | (static_cast<uint32_t>(get16(p + 2)) << 16);
}

} // namespace

void writeHeader(uint8_t *out, uint32_t dataBytes) {
    memcpy(out + 0, "RIFF", 4);
    put32(out + 4, static_cast<uint32_t>(kHeaderBytes - 8) + dataBytes);
    memcpy(out + 8, "WAVE", 4);

    memcpy(out + 12, "fmt ", 4);
    put32(out + 16, 20);
    put16(out + 20, kFormatImaAdpcm);
    put16(out + 22, kChannels);
    put32(out + 24, kSampleRate);
    put32(out + 28, kByteRate);
    put16(out + 32, static_cast<uint16_t>(ima::kBlockBytes));
    put16(out + 34, kBitsPerSample);
    put16(out + 36, 2); // cbSize: two extra bytes follow
    put16(out + 38, static_cast<uint16_t>(ima::kSamplesPerBlock));

    memcpy(out + 40, "fact", 4);
    put32(out + 44, 4);
    put32(out + 48, samplesForDataBytes(dataBytes));

    memcpy(out + 52, "data", 4);
    put32(out + 56, dataBytes);
}

bool readDataBytes(const uint8_t *in, uint32_t *dataBytes) {
    const bool ours = memcmp(in + 0, "RIFF", 4) == 0 &&
                      memcmp(in + 8, "WAVE", 4) == 0 &&
                      memcmp(in + 12, "fmt ", 4) == 0 &&
                      get16(in + 20) == kFormatImaAdpcm &&
                      get16(in + 22) == kChannels &&
                      get16(in + 32) == ima::kBlockBytes &&
                      memcmp(in + 52, "data", 4) == 0;
    if (!ours) {
        return false;
    }
    *dataBytes = get32(in + 56);
    return true;
}

uint32_t repairedDataBytes(uint32_t fileSize) {
    if (fileSize <= kHeaderBytes) {
        return 0;
    }
    const uint32_t blocks = (fileSize - kHeaderBytes) / ima::kBlockBytes;
    return blocks * ima::kBlockBytes;
}

uint32_t samplesForDataBytes(uint32_t dataBytes) {
    return dataBytes / ima::kBlockBytes * ima::kSamplesPerBlock;
}

uint32_t secondsForDataBytes(uint32_t dataBytes) {
    return samplesForDataBytes(dataBytes) / kSampleRate;
}

uint32_t secondsForFreeBytes(uint32_t freeBytes) {
    // Only whole blocks count: that is how much the recorder can write.
    return secondsForDataBytes(freeBytes);
}

} // namespace wav
