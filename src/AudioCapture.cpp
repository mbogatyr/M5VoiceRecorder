#include "AudioCapture.h"

bool AudioCapture::begin() {
    auto cfg = M5.Mic.config();
    cfg.sample_rate = kSampleRate;
    // The ES8311 runs at 8 kHz itself, so its own decimation filter keeps
    // everything above 4 kHz out; M5Unified's averaging is not needed.
    cfg.over_sampling = 1;
    // M5Unified's noise filter is a first-order low-pass that would dull
    // the voice further.
    cfg.noise_filter_level = 0;
    // M5Unified multiplies samples by magnification / (2 * over_sampling),
    // so 2 means unity gain. The ES8311 already applies +32 dB of digital
    // volume on its own.
    cfg.magnification = 2;
    // 8 descriptors of 256 frames; a mono capture reads two time steps per
    // frame, so the ring holds 4096 samples = 512 ms at 8 kHz.
    cfg.dma_buf_len = 256;
    cfg.dma_buf_count = 8;
    M5.Mic.config(cfg);

    if (!M5.Mic.isEnabled()) {
        return false;
    }

    // The speaker shares the I2S clock lines with the mic; setup() keeps it
    // off with cfg.internal_spk = false.
    return M5.Mic.begin();
}

const int16_t *AudioCapture::next() {
    while (M5.Mic.isRecording() < BlockRing::kInFlight) {
        if (!M5.Mic.record(ring_.nextWriteBlock(), kBlockLen, kSampleRate)) {
            break;
        }
        ring_.markQueued();
    }

    if (nextSeq_ >= ring_.completedBlocks()) {
        return nullptr;
    }
    const int16_t *block = ring_.block(nextSeq_);
    while (block == nullptr) {
        // The mic has already refilled this slot: skip to what is left.
        ++lost_;
        ++nextSeq_;
        block = ring_.block(nextSeq_);
    }
    ++nextSeq_;
    return block;
}
