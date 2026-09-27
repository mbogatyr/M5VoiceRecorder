#pragma once

#include <Arduino.h>
#include <FS.h>

#include "ImaAdpcm.h"
#include "Recordings.h"
#include "WavHeader.h"

// Writes one recording: IMA ADPCM WAV in LittleFS.
//
// Blocks are encoded as they arrive and written 16 at a time (4 KB, about
// a second of audio): fewer, larger writes mean fewer flash stalls. Every
// 10 s the file is synced, so a power cut loses at most that much; the
// header is filled in on stop(), or by Storage::repairAll() at the next
// start if the power went off first.
class RecordingWriter {
  public:
    static constexpr size_t kChunkBlocks = 16;
    static constexpr uint32_t kSyncMs = 10000;

    // budgetBytes: how much the file may grow before recording must stop.
    bool start(const char *name, uint32_t budgetBytes, uint32_t nowMs);

    // One capture block of ima::kSamplesPerBlock samples.
    void write(const int16_t *samples, uint32_t nowMs);

    void stop();

    bool active() const { return active_; }
    const char *name() const { return name_; }
    uint32_t dataBytes() const { return dataBytes_; }
    bool roomLeft() const;

    // The longest write()/sync seen, in ms, and a reset for the next
    // stats line.
    uint32_t takeMaxWriteMs();

  private:
    void writeChunk();

    File file_;
    ima::Encoder encoder_;
    uint8_t chunk_[kChunkBlocks * ima::kBlockBytes];
    size_t chunkBlocks_ = 0;
    char name_[rec::kNameSize] = "";
    uint32_t dataBytes_ = 0;
    uint32_t budgetBytes_ = 0;
    uint32_t lastSyncMs_ = 0;
    uint32_t maxWriteMs_ = 0;
    bool active_ = false;
};
