#include "LevelMeter.h"

#include <math.h>

uint8_t LevelMeter::levelForPeak(int32_t peak) {
    if (peak <= 0) {
        return 0;
    }
    const float db = 20.0f * log10f(static_cast<float>(peak) / 32768.0f);
    const float level = (db - kFloorDb) / -kFloorDb;
    if (level <= 0.0f) {
        return 0;
    }
    if (level >= 1.0f) {
        return 255;
    }
    return static_cast<uint8_t>(level * 255.0f + 0.5f);
}

void LevelMeter::push(const int16_t *samples, size_t count) {
    int32_t peak = 0;
    for (size_t i = 0; i < count; ++i) {
        const int32_t v = samples[i] < 0 ? -static_cast<int32_t>(samples[i]) : samples[i];
        if (v > peak) {
            peak = v;
        }
    }
    newest_ = (newest_ + 1) % kBars;
    bars_[newest_] = levelForPeak(peak);
    ++version_;
}

uint8_t LevelMeter::bar(size_t i) const {
    return bars_[(newest_ + 1 + i) % kBars];
}

void LevelMeter::clear() {
    for (auto &b : bars_) {
        b = 0;
    }
    newest_ = kBars - 1;
    ++version_;
}

void LevelMeter::toHex(char *out) const {
    static const char kDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < kBars; ++i) {
        const uint8_t v = bar(i);
        *out++ = kDigits[v >> 4];
        *out++ = kDigits[v & 0x0F];
    }
    *out = '\0';
}
