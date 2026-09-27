#include "Storage.h"

#include <LittleFS.h>
#include <Preferences.h>

#include <algorithm>

#include "ImaAdpcm.h"
#include "WavHeader.h"

namespace {

void pathFor(const char *name, char *out, size_t size) {
    snprintf(out, size, "/%s", name);
}

} // namespace

bool Storage::mount() { return LittleFS.begin(false); }

bool Storage::format() { return LittleFS.format() && LittleFS.begin(false); }

void Storage::repairAll() {
    File root = LittleFS.open("/");
    std::vector<String> names;
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        names.push_back(String(f.name()));
    }
    root.close();

    for (const String &name : names) {
        if (!rec::isRecordingName(name.c_str())) {
            continue;
        }
        char path[rec::kNameSize + 1];
        pathFor(name.c_str(), path, sizeof path);

        File f = LittleFS.open(path, "r+");
        if (!f) {
            continue;
        }
        const uint32_t size = f.size();
        const uint32_t actual = wav::repairedDataBytes(size);
        if (actual == 0) {
            f.close();
            LittleFS.remove(path);
            Serial.printf("repair: removed empty %s\n", path);
            continue;
        }
        uint8_t header[wav::kHeaderBytes];
        uint32_t written = 0;
        const bool readOk = f.read(header, sizeof header) == sizeof header &&
                            wav::readDataBytes(header, &written);
        if (!readOk || written != actual) {
            wav::writeHeader(header, actual);
            f.seek(0);
            f.write(header, sizeof header);
            Serial.printf("repair: %s data %u -> %u bytes\n", path,
                          static_cast<unsigned>(written), static_cast<unsigned>(actual));
        }
        f.close();
    }
}

void Storage::refresh() {
    total_ = LittleFS.totalBytes();
    const uint32_t used = LittleFS.usedBytes();
    free_ = total_ > used ? total_ - used : 0;

    recordings_.clear();
    File root = LittleFS.open("/");
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        if (f.isDirectory() || !rec::isRecordingName(f.name())) {
            continue;
        }
        Entry e;
        strlcpy(e.name, f.name(), sizeof e.name);
        e.bytes = f.size();
        recordings_.push_back(e);
    }
    root.close();
    std::sort(recordings_.begin(), recordings_.end(), [](const Entry &a, const Entry &b) {
        return rec::numberOf(a.name) > rec::numberOf(b.name);
    });
}

bool Storage::roomForRecording() const {
    // At least one full block of audio beyond the header must fit.
    return recordableBytes() >= wav::kHeaderBytes + 16 * ima::kBlockBytes;
}

uint32_t Storage::recordingsBytes() const {
    uint32_t sum = 0;
    for (const Entry &e : recordings_) {
        sum += e.bytes;
    }
    return sum;
}

void Storage::nextName(char *out) {
    Preferences prefs;
    prefs.begin("recorder", false);
    uint32_t last = prefs.getUInt("last", 0);
    for (const Entry &e : recordings_) {
        last = std::max(last, rec::numberOf(e.name));
    }

    // After 9999 the numbers start over; skip any still on the storage.
    // The list is enough here: LittleFS.exists() logs an error for every
    // missing file.
    uint32_t number = last;
    for (uint32_t tries = 0; tries < rec::kMaxNumber; ++tries) {
        number = rec::nextNumber(number);
        rec::nameFor(number, out);
        const bool taken = std::any_of(recordings_.begin(), recordings_.end(),
                                       [out](const Entry &e) { return strcmp(e.name, out) == 0; });
        if (!taken) {
            break;
        }
    }
    prefs.putUInt("last", number);
    prefs.end();
}

bool Storage::remove(const char *name) {
    if (!rec::isRecordingName(name)) {
        return false;
    }
    char path[rec::kNameSize + 1];
    pathFor(name, path, sizeof path);
    return LittleFS.remove(path);
}

uint32_t Storage::removeAll() {
    refresh();
    uint32_t removed = 0;
    for (const Entry &e : recordings_) {
        if (remove(e.name)) {
            ++removed;
        }
    }
    refresh();
    return removed;
}

void Storage::noteAdded(const char *name, uint32_t bytes) {
    Entry e;
    strlcpy(e.name, name, sizeof e.name);
    e.bytes = bytes;
    recordings_.insert(recordings_.begin(), e);
    constexpr uint32_t kBlock = 4096;
    const uint32_t used = (bytes + kBlock - 1) / kBlock * kBlock;
    free_ = free_ > used ? free_ - used : 0;
}
