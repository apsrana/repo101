#pragma once
// Minimal RIFF/WAVE header parser (PCM only).

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace furby {

struct WavInfo {
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataOffset = 0;  // byte offset of the first sample in the file
  uint32_t dataSize = 0;    // bytes of sample data
};

inline uint32_t readLe32(const uint8_t* p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline uint16_t readLe16(const uint8_t* p) { return p[0] | (p[1] << 8); }

// `buf` holds the start of the file (512 bytes is plenty for normal files).
// Returns false unless it is 8- or 16-bit PCM with a data chunk in range.
inline bool parseWav(const uint8_t* buf, size_t len, WavInfo& out) {
  if (len < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0)
    return false;
  bool haveFmt = false;
  size_t pos = 12;
  while (pos + 8 <= len) {
    const uint8_t* chunk = buf + pos;
    uint32_t size = readLe32(chunk + 4);
    if (memcmp(chunk, "fmt ", 4) == 0) {
      if (size < 16 || pos + 8 + 16 > len) return false;
      uint16_t format = readLe16(chunk + 8);
      if (format != 1) return false;  // PCM only
      out.channels = readLe16(chunk + 10);
      out.sampleRate = readLe32(chunk + 12);
      out.bitsPerSample = readLe16(chunk + 22);
      haveFmt = true;
    } else if (memcmp(chunk, "data", 4) == 0) {
      if (!haveFmt) return false;
      out.dataOffset = static_cast<uint32_t>(pos + 8);
      out.dataSize = size;
      return (out.bitsPerSample == 8 || out.bitsPerSample == 16) &&
             (out.channels == 1 || out.channels == 2) && out.sampleRate > 0;
    }
    pos += 8 + size + (size & 1);  // chunks are word aligned
  }
  return false;
}

}  // namespace furby
