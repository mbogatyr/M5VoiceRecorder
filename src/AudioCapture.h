#pragma once

#include <M5Unified.h>

#include "BlockRing.h"
#include "ImaAdpcm.h"
#include "WavHeader.h"

// Continuous capture from the StickS3's built-in mic (ES8311 codec) at
// 8 kHz.
//
// The mic records in blocks of two ADPCM blocks (1010 samples, 126 ms) in
// an M5Unified background task. next() keeps its queue full and hands out
// the finished blocks in order.
//
// Why two and not one: an ADPCM block is 505 samples, an odd number, and
// M5Unified's mono capture produces samples in pairs. With odd-length
// requests it carries the second sample of a pair over to the next
// request, and on the board that path left one wrong sample at a block
// start every 74 blocks (a faint tick every 4.7 s). Even-length requests
// never need the carry.
//
// Flash writes stop both cores for tens of milliseconds. The audio is not
// lost meanwhile: the I2S DMA ring (512 ms here) keeps filling, and the mic
// task catches up from it once the flash is done.
class AudioCapture {
  public:
    static constexpr uint32_t kSampleRate = wav::kSampleRate;
    static constexpr size_t kAdpcmBlocksPerCapture = 2;
    static constexpr size_t kBlockLen = ima::kSamplesPerBlock * kAdpcmBlocksPerCapture;
    // Two blocks in the mic queue plus room for the loop to fall behind.
    static constexpr size_t kRingBlocks = BlockRing::kInFlight + 4;

    // Call after M5.begin(). Returns false if the board has no mic.
    bool begin();

    // Tops up the mic queue and returns the next finished block, or nullptr
    // when there is none yet. Call it until it returns nullptr.
    const int16_t *next();

    // Blocks whose slot was reused before they were read: audio that was
    // really lost. Stays 0 unless the loop stalls for about half a second.
    uint32_t lostBlocks() const { return lost_; }

    // Finished blocks handed out so far.
    uint32_t deliveredBlocks() const { return nextSeq_ - lost_; }

  private:
    BlockRing ring_{kBlockLen, kRingBlocks};
    uint32_t nextSeq_ = 0;
    uint32_t lost_ = 0;
};
