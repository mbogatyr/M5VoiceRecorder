#pragma once

#include <stddef.h>
#include <stdint.h>

// Names of recordings and the numbers shown next to them.
//
// There is no real-time clock on the StickS3, so recordings are numbered:
// REC_0001.wav, REC_0002.wav, ... The name is also the only thing the web
// page may ask for, so isRecordingName() doubles as the guard that keeps a
// request from reaching any other file.
namespace rec {

constexpr uint32_t kMaxNumber = 9999;
constexpr size_t kNameSize = sizeof("REC_0000.wav");

// Exactly "REC_" + four digits + ".wav"; no paths, nothing else.
bool isRecordingName(const char *name);

// The number in a recording name, or 0 if the name is not one.
uint32_t numberOf(const char *name);

// out must hold kNameSize bytes. number is 1..kMaxNumber.
void nameFor(uint32_t number, char *out);

// The number after `last`, starting over at 1 after kMaxNumber.
uint32_t nextNumber(uint32_t last);

// "mm:ss" under an hour, "h:mm:ss" from an hour on.
void formatDuration(uint32_t seconds, char *out, size_t size);

// Decimal megabytes: "0.33 MB", "12.4 MB".
void formatMegabytes(uint32_t bytes, char *out, size_t size);

} // namespace rec
