#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ImaAdpcm.h"

// The WAV header of a recording: IMA ADPCM, mono, 8 kHz.
//
// Layout (60 bytes, little-endian):
//   RIFF <size> WAVE
//   fmt  20: tag 0x11, 1 channel, rate, byte rate, block align, 4 bits,
//            cbSize 2, samples per block
//   fact  4: total samples
//   data <size>
//
// The recorder writes the header with a zero data size when it opens a
// file and fills in the real sizes when it closes it. If the power goes
// off in between, the header stays wrong; repairedDataBytes() works out
// the right size from the length of the file.
namespace wav {

constexpr uint32_t kSampleRate = 8000;
constexpr size_t kHeaderBytes = 60;

// Writes the header for a data chunk of dataBytes (whole ADPCM blocks).
void writeHeader(uint8_t *out, uint32_t dataBytes);

// Reads the data size from a header written by writeHeader(). Returns
// false if the bytes are not a header in this format.
bool readDataBytes(const uint8_t *in, uint32_t *dataBytes);

// The data size a file of fileSize bytes actually holds: whole blocks
// after the header. A block cut short by a power loss is dropped.
uint32_t repairedDataBytes(uint32_t fileSize);

uint32_t samplesForDataBytes(uint32_t dataBytes);

// Duration, rounded down to whole seconds, as the screen and the web page
// show it.
uint32_t secondsForDataBytes(uint32_t dataBytes);

// Recording time that fits into freeBytes, in whole seconds.
uint32_t secondsForFreeBytes(uint32_t freeBytes);

} // namespace wav
