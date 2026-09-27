#pragma once

#include <stdint.h>

enum class Backlight : uint8_t { On, Dim, Off };

// Decides how bright the display should be.
//
// Without key presses the display turns off after 3 minutes, which saves
// the battery. While recording it never turns off, only dims after 30
// seconds: a recorder should always show that it is recording.
//
// It does not deal with powering off: the StickS3 power button turns the
// board off on a double press by itself, in the PMIC, without the firmware.
//
// Like everything in lib/, it does not touch the hardware: the time and
// whether anything happened go in, a decision comes out.
class ScreenPolicy {
  public:
    static constexpr uint32_t kOffMs = 180000; // 3 minutes
    static constexpr uint32_t kDimMs = 30000;  // 30 seconds, while recording

    explicit ScreenPolicy(uint32_t offMs = kOffMs, uint32_t dimMs = kDimMs);

    // Sets the point the idle time is counted from. Call once at startup.
    void begin(uint32_t nowMs);

    // activity: a key press or a web request in this tick.
    Backlight update(uint32_t nowMs, bool activity, bool recording);

  private:
    uint32_t offMs_;
    uint32_t dimMs_;
    uint32_t lastActivityMs_ = 0;
};
