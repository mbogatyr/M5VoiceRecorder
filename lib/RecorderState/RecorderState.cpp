#include "RecorderState.h"

void RecorderState::begin(uint32_t nowMs) {
    current_ = previous_ = View{Mode::Ready, false};
    sinceMs_ = nowMs;
    stoppedFull_ = false;
}

void RecorderState::enter(Mode mode, bool wifi, uint32_t nowMs) {
    previous_ = current_;
    current_ = View{mode, wifi};
    sinceMs_ = nowMs;
}

Command RecorderState::update(uint32_t nowMs, const Input &in) {
    const bool wifi = current_.wifi;

    switch (current_.mode) {
    case Mode::Ready:
        if (wifi) {
            // In Wi-Fi mode recording starts from the web page only; screen
            // 4 shows no KEY1 hint.
            if (in.webRecord && in.roomLeft) {
                enter(Mode::Recording, true, nowMs);
                return Command::StartRecording;
            }
            if (in.key2) {
                enter(Mode::Ready, false, nowMs);
                return Command::WifiOff;
            }
        } else {
            if (in.key1 && in.roomLeft) {
                enter(Mode::Recording, false, nowMs);
                return Command::StartRecording;
            }
            if (in.key2) {
                enter(Mode::Ready, true, nowMs);
                return Command::WifiOn;
            }
        }
        break;

    case Mode::Recording:
        // KEY1 stops on screens 2 and 8 alike; KEY2 does nothing here.
        if (in.key1 || in.webStop || !in.roomLeft) {
            stoppedFull_ = !(in.key1 || in.webStop);
            enter(Mode::Saved, wifi, nowMs);
            return Command::StopRecording;
        }
        break;

    case Mode::Saved:
        // The web page shows Record while the stick shows the summary.
        if (in.webRecord && in.roomLeft) {
            enter(Mode::Recording, wifi, nowMs);
            return Command::StartRecording;
        }
        // Screen 3 shows no button hints, so a key only cuts it short.
        if (in.key1 || in.key2 || nowMs - sinceMs_ >= kSavedMs) {
            enter(Mode::Ready, wifi, nowMs);
        }
        break;
    }
    return Command::None;
}

void RecorderState::cancelRecording(uint32_t nowMs) {
    if (current_.mode == Mode::Recording) {
        enter(Mode::Ready, current_.wifi, nowMs);
    }
}

bool RecorderState::fades(View from, View to) {
    if (from.mode == Mode::Saved && to.mode == Mode::Ready) {
        return true;
    }
    return from.mode == Mode::Ready && to.mode == Mode::Ready && from.wifi != to.wifi;
}

Fade RecorderState::fade(uint32_t nowMs) const {
    const uint32_t elapsed = nowMs - sinceMs_;
    if (!fades(previous_, current_) || elapsed >= 2 * kFadeHalfMs) {
        return {false, 255};
    }
    if (elapsed < kFadeHalfMs) {
        const uint32_t left = kFadeHalfMs - elapsed;
        return {true, static_cast<uint8_t>(255 * left / kFadeHalfMs)};
    }
    const uint32_t shown = elapsed - kFadeHalfMs;
    return {false, static_cast<uint8_t>(255 * shown / kFadeHalfMs)};
}
