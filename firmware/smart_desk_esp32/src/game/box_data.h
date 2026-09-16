#pragma once
#include "pokemon_record_codec.h"

namespace PokemonGame {
constexpr uint16_t BOX_FORMAT_VERSION = 1;
constexpr uint32_t BOX_CAPACITY = 2048;
constexpr size_t BOX_HEADER_SIZE = 64;
constexpr size_t BOX_BITMAP_SIZE = BOX_CAPACITY / 8;
constexpr size_t BOX_PAYLOAD_SIZE = BOX_BITMAP_SIZE + BOX_CAPACITY * POKEMON_RECORD_SIZE;
constexpr size_t BOX_FILE_SIZE = BOX_HEADER_SIZE + BOX_PAYLOAD_SIZE;
constexpr size_t BOX_CRC_OFFSET = 44;
constexpr size_t BOX_RECORDS_OFFSET = BOX_HEADER_SIZE + BOX_BITMAP_SIZE;

struct BoxKey {
  uint64_t storeId = 0;
  uint64_t generation = 0;
};
struct BoxMetadata {
  BoxKey key;
  uint32_t occupiedCount = 0;
  uint32_t crc32 = 0;
};
enum class BoxMutationKind { Insert, Remove, Replace };
struct BoxMutation {
  BoxMutationKind kind = BoxMutationKind::Insert;
  uint32_t slot = 0;
  PokemonInstance pokemon;
};
// Wire offsets: magic 0, version 4, header 6, record 8, flags 10,
// capacity 12, count 16, bitmap bytes 20, store 24, generation 32,
// payload bytes 40, CRC 44, reserved 48..63. Reserved bytes must be zero.
void encodeBoxHeader(const BoxMetadata& metadata, uint8_t* output);
bool decodeBoxHeader(const uint8_t* input, BoxMetadata& output);
bool boxOccupied(const uint8_t* bitmap, uint32_t slot);
void setBoxOccupied(uint8_t* bitmap, uint32_t slot, bool occupied);
}
