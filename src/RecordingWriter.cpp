#include "RecordingWriter.h"

#include <LittleFS.h>

bool RecordingWriter::start(const char *name, uint32_t budgetBytes, uint32_t nowMs) {
    char path[rec::kNameSize + 1];
    snprintf(path, sizeof path, "/%s", name);
    file_ = LittleFS.open(path, "w");
    if (!file_) {
        return false;
    }

    uint8_t header[wav::kHeaderBytes];
    wav::writeHeader(header, 0);
    if (file_.write(header, sizeof header) != sizeof header) {
        file_.close();
        LittleFS.remove(path);
        return false;
    }

    strlcpy(name_, name, sizeof name_);
    encoder_.reset();
    chunkBlocks_ = 0;
    dataBytes_ = 0;
    budgetBytes_ = budgetBytes > wav::kHeaderBytes ? budgetBytes - wav::kHeaderBytes : 0;
    lastSyncMs_ = nowMs;
    maxWriteMs_ = 0;
    active_ = true;
    return true;
}

bool RecordingWriter::roomLeft() const {
    return dataBytes_ + ima::kBlockBytes <= budgetBytes_;
}

void RecordingWriter::write(const int16_t *samples, uint32_t nowMs) {
    if (!active_ || !roomLeft()) {
        return;
    }
    encoder_.encodeBlock(samples, chunk_ + chunkBlocks_ * ima::kBlockBytes);
    ++chunkBlocks_;
    dataBytes_ += ima::kBlockBytes;

    if (chunkBlocks_ == kChunkBlocks) {
        writeChunk();
    }
    if (nowMs - lastSyncMs_ >= kSyncMs) {
        const uint32_t start = millis();
        file_.flush(); // fflush + fsync: the size so far survives a power cut
        const uint32_t took = millis() - start;
        maxWriteMs_ = took > maxWriteMs_ ? took : maxWriteMs_;
        lastSyncMs_ = nowMs;
    }
}

void RecordingWriter::writeChunk() {
    if (chunkBlocks_ == 0) {
        return;
    }
    const uint32_t start = millis();
    file_.write(chunk_, chunkBlocks_ * ima::kBlockBytes);
    const uint32_t took = millis() - start;
    maxWriteMs_ = took > maxWriteMs_ ? took : maxWriteMs_;
    chunkBlocks_ = 0;
}

void RecordingWriter::stop() {
    if (!active_) {
        return;
    }
    writeChunk();

    uint8_t header[wav::kHeaderBytes];
    wav::writeHeader(header, dataBytes_);
    file_.seek(0);
    file_.write(header, sizeof header);
    file_.close();
    active_ = false;
}

uint32_t RecordingWriter::takeMaxWriteMs() {
    const uint32_t v = maxWriteMs_;
    maxWriteMs_ = 0;
    return v;
}
