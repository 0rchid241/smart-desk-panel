#include "box_data.h"
#include <cstring>

namespace PokemonGame {
namespace {
void put(uint8_t* p, uint64_t value, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i) { p[i] = static_cast<uint8_t>(value); value >>= 8; }
}
uint64_t get(const uint8_t* p, unsigned bytes) {
  uint64_t value = 0;
  for (unsigned i = 0; i < bytes; ++i) value |= static_cast<uint64_t>(p[i]) << (8 * i);
  return value;
}
}
void encodeBoxHeader(const BoxMetadata& m, uint8_t* p) {
  std::memset(p, 0, BOX_HEADER_SIZE);
  std::memcpy(p, "PKBX", 4);
  put(p + 4, BOX_FORMAT_VERSION, 2); put(p + 6, BOX_HEADER_SIZE, 2);
  put(p + 8, POKEMON_RECORD_SIZE, 2); put(p + 12, BOX_CAPACITY, 4);
  put(p + 16, m.occupiedCount, 4); put(p + 20, BOX_BITMAP_SIZE, 4);
  put(p + 24, m.key.storeId, 8); put(p + 32, m.key.generation, 8);
  put(p + 40, BOX_PAYLOAD_SIZE, 4); put(p + BOX_CRC_OFFSET, m.crc32, 4);
}
bool decodeBoxHeader(const uint8_t* p, BoxMetadata& output) {
  if (std::memcmp(p, "PKBX", 4) || get(p + 4, 2) != BOX_FORMAT_VERSION ||
      get(p + 6, 2) != BOX_HEADER_SIZE || get(p + 8, 2) != POKEMON_RECORD_SIZE ||
      get(p + 10, 2) != 0 || get(p + 12, 4) != BOX_CAPACITY ||
      get(p + 16, 4) > BOX_CAPACITY || get(p + 20, 4) != BOX_BITMAP_SIZE ||
      get(p + 40, 4) != BOX_PAYLOAD_SIZE) return false;
  for (size_t i = 48; i < BOX_HEADER_SIZE; ++i) if (p[i]) return false;
  BoxMetadata m;
  m.occupiedCount = static_cast<uint32_t>(get(p + 16, 4));
  m.key = {get(p + 24, 8), get(p + 32, 8)};
  m.crc32 = static_cast<uint32_t>(get(p + BOX_CRC_OFFSET, 4));
  if (!m.key.storeId || !m.key.generation) return false;
  output = m;
  return true;
}
bool boxOccupied(const uint8_t* bitmap, uint32_t slot) {
  return slot < BOX_CAPACITY && (bitmap[slot / 8] & (1u << (slot % 8))) != 0;
}
void setBoxOccupied(uint8_t* bitmap, uint32_t slot, bool occupied) {
  if (slot >= BOX_CAPACITY) return;
  const uint8_t mask = static_cast<uint8_t>(1u << (slot % 8));
  if (occupied) bitmap[slot / 8] |= mask;
  else bitmap[slot / 8] &= static_cast<uint8_t>(~mask);
}
}
