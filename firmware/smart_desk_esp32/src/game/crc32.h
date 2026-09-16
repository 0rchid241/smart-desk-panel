#pragma once
#include <cstddef>
#include <cstdint>

namespace PokemonGame {
// IEEE CRC32: initialize with 0xffffffff, finish with bitwise complement.
inline uint32_t updateCrc32(uint32_t crc, const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
  }
  return crc;
}
}
