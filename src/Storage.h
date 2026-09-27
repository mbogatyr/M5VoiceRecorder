#pragma once

#include <Arduino.h>

#include <vector>

#include "Recordings.h"

// The recordings in the LittleFS partition ("spiffs" label, ~5.9 MB).
//
// LittleFS rather than FAT: it survives a power cut in the middle of a
// write, which happens whenever the power button is double-pressed while
// recording.
class Storage {
  public:
    // Keeps this much free for LittleFS's own metadata; recording stops
    // when only this is left.
    static constexpr uint32_t kReserveBytes = 64 * 1024;

    struct Entry {
        char name[rec::kNameSize];
        uint32_t bytes;
    };

    // Mounts the partition. Returns false if it holds no file system yet
    // (the first start): then call format().
    bool mount();
    bool format();

    // Fixes the headers of recordings cut short by a power loss and
    // removes files too short to hold any audio.
    void repairAll();

    // Re-reads free space and the list of recordings. The free space query
    // walks the whole file system, so it runs only when something changed,
    // not every loop.
    void refresh();

    uint32_t totalBytes() const { return total_; }
    uint32_t freeBytes() const { return free_; }
    // Free space the recorder may still fill.
    uint32_t recordableBytes() const {
        return free_ > kReserveBytes ? free_ - kReserveBytes : 0;
    }
    bool roomForRecording() const;

    // Recordings, newest (highest number) first.
    const std::vector<Entry> &recordings() const { return recordings_; }
    uint32_t recordingsBytes() const;

    // The name for a new recording. Numbers only go up, even after
    // deletions (the last one is kept in NVS), so a new file never gets the
    // name of one already downloaded.
    void nextName(char *out);

    bool remove(const char *name);

  private:
    uint32_t total_ = 0;
    uint32_t free_ = 0;
    std::vector<Entry> recordings_;
};
