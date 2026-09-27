#pragma once

#include <stdint.h>

// What the recorder is doing, and which screen goes with it.
//
//   Ready     screen 1 (screen 5 when memory is full)
//   Recording screen 2
//   Saved     screen 3: a summary that turns into Ready after 4 s
//   Wifi      screen 4
//
// Like everything in lib/, it does not touch the hardware: key presses,
// the time and whether there is room for more audio go in; the mode and a
// command for main.cpp to carry out come out.
enum class Mode : uint8_t { Ready, Recording, Saved, Wifi };

enum class Command : uint8_t { None, StartRecording, StopRecording, WifiOn, WifiOff };

// How to draw a mode change that fades: first the previous screen goes
// dark, then the new one comes up.
struct Fade {
    bool showPrevious; // draw what the previous mode showed
    uint8_t level;     // 0 is black, 255 is full brightness
};

class RecorderState {
  public:
    static constexpr uint32_t kSavedMs = 4000;   // how long screen 3 stays
    static constexpr uint32_t kFadeHalfMs = 300; // fade out, then fade in

    void begin(uint32_t nowMs);

    // key1, key2: pressed in this tick. roomLeft: another block of audio
    // still fits into the storage.
    Command update(uint32_t nowMs, bool key1, bool key2, bool roomLeft);

    // main.cpp could not start the recording (for example, the file did
    // not open): back to Ready.
    void cancelRecording(uint32_t nowMs);

    Mode mode() const { return mode_; }
    Mode previous() const { return previous_; }

    // The last recording stopped because the memory ran out, not by KEY1.
    bool stoppedFull() const { return stoppedFull_; }

    // When the current mode began.
    uint32_t sinceMs() const { return sinceMs_; }

    Fade fade(uint32_t nowMs) const;

    // Which mode changes fade: Saved -> Ready and Ready <-> Wifi. Starting
    // and stopping a recording answer the key at once.
    static bool fades(Mode from, Mode to);

  private:
    void enter(Mode mode, uint32_t nowMs);

    Mode mode_ = Mode::Ready;
    Mode previous_ = Mode::Ready;
    uint32_t sinceMs_ = 0;
    bool stoppedFull_ = false;
};
