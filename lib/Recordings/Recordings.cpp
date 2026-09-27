#include "Recordings.h"

#include <stdio.h>
#include <string.h>

namespace rec {

bool isRecordingName(const char *name) { return numberOf(name) != 0; }

uint32_t numberOf(const char *name) {
    if (name == nullptr || strlen(name) != kNameSize - 1 ||
        strncmp(name, "REC_", 4) != 0 || strcmp(name + 8, ".wav") != 0) {
        return 0;
    }
    uint32_t number = 0;
    for (int i = 4; i < 8; ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return 0;
        }
        number = number * 10 + static_cast<uint32_t>(name[i] - '0');
    }
    return number;
}

void nameFor(uint32_t number, char *out) {
    snprintf(out, kNameSize, "REC_%04u.wav", static_cast<unsigned>(number % 10000));
}

uint32_t nextNumber(uint32_t last) {
    return (last >= kMaxNumber) ? 1 : last + 1;
}

void formatDuration(uint32_t seconds, char *out, size_t size) {
    const unsigned h = seconds / 3600;
    const unsigned m = (seconds / 60) % 60;
    const unsigned s = seconds % 60;
    if (h > 0) {
        snprintf(out, size, "%u:%02u:%02u", h, m, s);
    } else {
        snprintf(out, size, "%02u:%02u", m, s);
    }
}

void formatMegabytes(uint32_t bytes, char *out, size_t size) {
    // Rounded to hundredths (tenths from 10 MB on) in integers, so the
    // result does not depend on float formatting.
    if (bytes < 10000000u) {
        const unsigned hundredths = (bytes + 5000u) / 10000u;
        snprintf(out, size, "%u.%02u MB", hundredths / 100, hundredths % 100);
    } else {
        const unsigned tenths = (bytes + 50000u) / 100000u;
        snprintf(out, size, "%u.%u MB", tenths / 10, tenths % 10);
    }
}

} // namespace rec
