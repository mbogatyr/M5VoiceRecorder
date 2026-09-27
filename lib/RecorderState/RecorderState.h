#pragma once

#include <stdint.h>

// What the recorder is doing, and which screen goes with it.
//
//   Ready     screen 1 (5 when memory is full); with Wi-Fi on: 4 (9)
//   Recording screen 2; with Wi-Fi on: 8
//   Saved     screen 3: a summary that turns into Ready after 4 s
//
// Wi-Fi is a separate switch, not a mode: a recording started from the web
// page runs with the access point on.
//
// Like everything in lib/, it does not touch the hardware: key presses,
// requests from the web page, the time and whether there is room for more
// audio go in; the state and a command for main.cpp come out.
enum class Mode : uint8_t { Ready, Recording, Saved };

enum class Command : uint8_t { None, StartRecording, StopRecording, WifiOn, WifiOff };

// A mode together with the Wi-Fi switch: what the screen shows.
struct View {
    Mode mode;
    bool wifi;
};

// Everything that happened in one tick.
struct Input {
    bool key1 = false;      // KEY1 pressed
    bool key2 = false;      // KEY2 pressed
    bool webRecord = false; // Record pressed on the web page
    bool webStop = false;   // Stop pressed on the web page
    bool roomLeft = true;   // another block of audio still fits
};

// How to draw a view change that fades: first the previous screen goes
// dark, then the new one comes up.
struct Fade {
    bool showPrevious; // draw what the previous view showed
    uint8_t level;     // 0 is black, 255 is full brightness
};

class RecorderState {
  public:
    static constexpr uint32_t kSavedMs = 4000;   // how long screen 3 stays
    static constexpr uint32_t kFadeHalfMs = 300; // fade out, then fade in

    void begin(uint32_t nowMs);

    Command update(uint32_t nowMs, const Input &input);

    // main.cpp could not start the recording (for example, the file did
    // not open): back to Ready.
    void cancelRecording(uint32_t nowMs);

    Mode mode() const { return current_.mode; }
    bool wifi() const { return current_.wifi; }
    View view() const { return current_; }
    View previous() const { return previous_; }

    // The last recording stopped because the memory ran out, not by a key
    // or the web page.
    bool stoppedFull() const { return stoppedFull_; }

    // When the current view began.
    uint32_t sinceMs() const { return sinceMs_; }

    Fade fade(uint32_t nowMs) const;

    // Which view changes fade: Saved -> Ready, and Ready <-> Ready when
    // Wi-Fi goes on or off (screens 1 <-> 4). Starting and stopping a
    // recording answer at once.
    static bool fades(View from, View to);

  private:
    void enter(Mode mode, bool wifi, uint32_t nowMs);

    View current_{Mode::Ready, false};
    View previous_{Mode::Ready, false};
    uint32_t sinceMs_ = 0;
    bool stoppedFull_ = false;
};
