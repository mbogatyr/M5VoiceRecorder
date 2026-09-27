#include "ScreenPolicy.h"

ScreenPolicy::ScreenPolicy(uint32_t offMs, uint32_t dimMs)
    : offMs_(offMs), dimMs_(dimMs) {}

void ScreenPolicy::begin(uint32_t nowMs) { lastActivityMs_ = nowMs; }

Backlight ScreenPolicy::update(uint32_t nowMs, bool activity, bool recording) {
    if (activity) {
        lastActivityMs_ = nowMs;
    }
    // Unsigned subtraction handles the millis() rollover correctly.
    const uint32_t idle = nowMs - lastActivityMs_;
    if (recording) {
        return idle < dimMs_ ? Backlight::On : Backlight::Dim;
    }
    return idle < offMs_ ? Backlight::On : Backlight::Off;
}
