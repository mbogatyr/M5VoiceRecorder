#pragma once

#include <M5Unified.h>

#include "LevelMeter.h"
#include "Recordings.h"

// The numbered screens of the approved design.
enum class ScreenId : uint8_t {
    Ready = 1,     // 1
    Recording = 2, // 2
    Saved = 3,     // 3
    Wifi = 4,      // 4
    Full = 5,      // 5: Ready with no room left
    Preparing = 6, // 6: formatting the storage on the first start
    NoMic = 7,     // 7
    RecordingWifi = 8, // 8: screen 2 while the access point is on
    WifiFull = 9,      // 9: screen 4 with no room left
};

// Everything a frame shows. main.cpp fills it (after zeroing it with
// memset, so that two equal models compare equal byte for byte) and the
// renderer skips frames whose model has not changed.
struct ScreenModel {
    ScreenId screen;
    ScreenId previous;  // shown while the old screen fades out
    bool showPrevious;  // the fade is in its first, darkening half
    uint8_t fadeLevel;  // 255 = full brightness, 0 = black

    uint8_t batteryPercent;
    bool wifi; // the access point is on: a small Wi-Fi mark by the battery

    uint32_t secondsLeft;
    uint32_t freeBytes;
    uint32_t totalBytes;
    uint32_t recordingCount;
    uint32_t recordingsBytes;

    // Screen 2
    uint32_t elapsedSeconds;
    uint32_t recordedBytes;
    uint32_t meterVersion;
    uint8_t ringPhase; // step of the pulsing ring animation, 0..kRingSteps-1

    // Screen 3
    char savedName[rec::kNameSize];
    uint32_t savedSeconds;
    uint32_t savedBytes;
    bool savedFull;

    // Screen 4
    uint8_t stations;
};

// Draws on the StickS3 display turned on its side (240x135), KEY1 to the
// right of the screen, KEY2 on the top edge above the screen's right third.
//
// The whole frame is composed in a sprite and pushed in a single call:
// drawing directly on the screen causes visible flicker.
class Renderer {
  public:
    static constexpr uint32_t kRingPeriodMs = 1400;
    static constexpr uint8_t kRingSteps = 28; // 50 ms per step

    // Call after M5.begin().
    void begin();

    // Redraws the screen only when the model has changed. The meter is
    // read on screen 2 only.
    void draw(const ScreenModel &model, const LevelMeter &meter);

    // Forgets the last frame. Needed after the display wakes up: its
    // contents are lost during sleep.
    void invalidate() { hasPrevious_ = false; }

    // Writes the last drawn frame: a "SNAP <width> <height>" line, then the
    // RGB565 pixels, two bytes each, high byte first. tools/serial_cmd.py
    // turns this into a PNG.
    void writeSnapshot(Print &out);

  private:
    void paint(const ScreenModel &m, const LevelMeter &meter);

    void paintBattery(const ScreenModel &m);
    void paintReady(const ScreenModel &m, bool full);
    void paintRecording(const ScreenModel &m, const LevelMeter &meter);
    void paintSaved(const ScreenModel &m);
    void paintWifi(const ScreenModel &m, bool full);
    void paintMessage(bool error);

    // KEY1 zone at the right edge, KEY2 hint on the top row.
    void paintKey1Arrow();
    void paintKey2Hint(const char *label);

    // Colour dimmed by the current fade level.
    uint16_t col(uint32_t rgb) const;
    uint16_t colScaled(uint32_t rgb, uint16_t scale) const; // scale 0..256

    // Draws text and returns its width. bold: drawn twice, 1 px apart.
    int text(const char *s, int x, int y, const lgfx::IFont *font, uint32_t rgb,
             textdatum_t datum = top_left, bool bold = false);
    int width(const char *s, const lgfx::IFont *font, bool bold = false);
    // A small centred dot between two parts of a line; returns its width.
    int separator(int x, int y, int height, uint32_t rgb);

    void bar(int x, int y, int w, int h, uint32_t filled, uint32_t total, uint32_t rgb);
    void micIcon(int cx, int cy, uint32_t rgb, bool crossed);

    M5Canvas canvas_{&M5.Display};
    uint8_t level_ = 255;

    bool hasPrevious_ = false;
    ScreenModel previousModel_{};
};
