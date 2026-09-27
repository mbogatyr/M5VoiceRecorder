#pragma once

#include <stddef.h>
#include <stdint.h>

// The waveform on screen 2: one bar per captured block, newest on the
// right. A bar is the block's peak on a logarithmic scale from -60 dBFS
// (empty) to 0 dBFS (full height), so quiet speech still shows.
class LevelMeter {
  public:
    static constexpr size_t kBars = 54;
    static constexpr float kFloorDb = -60.0f;

    // Adds a bar for a block of samples.
    void push(const int16_t *samples, size_t count);

    // Bar height 0..255; i = 0 is the oldest, kBars - 1 the newest.
    uint8_t bar(size_t i) const;

    // Empties the history (a new recording starts with a flat line).
    void clear();

    // The bars, oldest first, as 2 * kBars lowercase hex characters plus a
    // terminating zero, for the web page's copy of the waveform.
    void toHex(char *out) const;

    // Bumps on every push(), so the renderer knows the bars moved.
    uint32_t version() const { return version_; }

    // Bar height for a peak sample value.
    static uint8_t levelForPeak(int32_t peak);

  private:
    uint8_t bars_[kBars] = {};
    size_t newest_ = kBars - 1;
    uint32_t version_ = 0;
};
