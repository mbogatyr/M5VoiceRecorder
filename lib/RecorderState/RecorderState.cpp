#include "RecorderState.h"

void RecorderState::begin(uint32_t nowMs) {
    mode_ = Mode::Ready;
    previous_ = Mode::Ready;
    sinceMs_ = nowMs;
    stoppedFull_ = false;
}

void RecorderState::enter(Mode mode, uint32_t nowMs) {
    previous_ = mode_;
    mode_ = mode;
    sinceMs_ = nowMs;
}

Command RecorderState::update(uint32_t nowMs, bool key1, bool key2, bool roomLeft) {
    switch (mode_) {
    case Mode::Ready:
        if (key1 && roomLeft) {
            enter(Mode::Recording, nowMs);
            return Command::StartRecording;
        }
        if (key2) {
            enter(Mode::Wifi, nowMs);
            return Command::WifiOn;
        }
        break;

    case Mode::Recording:
        if (key1 || !roomLeft) {
            stoppedFull_ = !key1;
            enter(Mode::Saved, nowMs);
            return Command::StopRecording;
        }
        break;

    case Mode::Saved:
        // Screen 3 shows no button hints, so a key only cuts it short.
        if (key1 || key2 || nowMs - sinceMs_ >= kSavedMs) {
            enter(Mode::Ready, nowMs);
        }
        break;

    case Mode::Wifi:
        if (key2) {
            enter(Mode::Ready, nowMs);
            return Command::WifiOff;
        }
        break;
    }
    return Command::None;
}

void RecorderState::cancelRecording(uint32_t nowMs) {
    if (mode_ == Mode::Recording) {
        enter(Mode::Ready, nowMs);
    }
}

bool RecorderState::fades(Mode from, Mode to) {
    return (from == Mode::Saved && to == Mode::Ready) ||
           (from == Mode::Ready && to == Mode::Wifi) ||
           (from == Mode::Wifi && to == Mode::Ready);
}

Fade RecorderState::fade(uint32_t nowMs) const {
    const uint32_t elapsed = nowMs - sinceMs_;
    if (!fades(previous_, mode_) || elapsed >= 2 * kFadeHalfMs) {
        return {false, 255};
    }
    if (elapsed < kFadeHalfMs) {
        const uint32_t left = kFadeHalfMs - elapsed;
        return {true, static_cast<uint8_t>(255 * left / kFadeHalfMs)};
    }
    const uint32_t shown = elapsed - kFadeHalfMs;
    return {false, static_cast<uint8_t>(255 * shown / kFadeHalfMs)};
}
