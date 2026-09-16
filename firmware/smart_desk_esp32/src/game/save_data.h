#pragma once
#include "game_state.h"
#include "pokemon_record_codec.h"
#include "box_data.h"

namespace PokemonGame {
constexpr uint16_t SAVE_VERSION = 5;
constexpr size_t SAVE_HEADER_SIZE = 18;
constexpr size_t SAVE_V1_MAX_SIZE =
  SAVE_HEADER_SIZE + 14 + 1 +
  PARTY_CAPACITY * POKEMON_RECORD_SIZE +
  3 * POKEDEX_BYTES;
constexpr size_t EXPLORATION_RECORD_SIZE = 15;
constexpr size_t SAVE_V2_MAX_SIZE =
  SAVE_V1_MAX_SIZE +
  EXPLORATION_RECORD_SIZE;
constexpr size_t ENCOUNTER_RECORD_SIZE = 7;
constexpr size_t SAVE_V3_MAX_SIZE =
  SAVE_V2_MAX_SIZE +
  ENCOUNTER_RECORD_SIZE;
constexpr size_t BATTLE_RECORD_SIZE = 40;
constexpr size_t SAVE_V4_MAX_SIZE = SAVE_V3_MAX_SIZE + BATTLE_RECORD_SIZE;
constexpr size_t BOX_ROOT_RECORD_SIZE = 32;
constexpr size_t SAVE_MAX_SIZE = SAVE_V4_MAX_SIZE + BOX_ROOT_RECORD_SIZE;
struct BoxRoot {
  uint64_t storeId = 0;
  uint64_t generation = 0;
  uint32_t capacity = 0;
  uint32_t occupiedCount = 0;
  uint32_t snapshotCrc32 = 0;
  uint16_t boxFormatVersion = 0;
  uint16_t flags = 0;
};
bool isValidBoxRoot(const BoxRoot& root);
BoxRoot boxRootFromMetadata(const BoxMetadata& metadata);
struct GameSave {
  uint16_t saveVersion = SAVE_VERSION;
  uint32_t sequence = 0;
  GameState state;
  BoxRoot boxRoot; // Persistence transaction metadata, not game runtime state.
};
enum class DecodeResult { Ok, Invalid, UnsupportedVersion };
// Fixed little-endian wire format, independent of compiler padding or Arduino.
bool serialize(const GameSave& save, uint8_t* output, size_t capacity, size_t& written);
// Older saves remain readable:
// v1 -> Idle exploration + empty encounter
// v2 -> existing exploration + empty encounter
// v3 -> existing exploration/encounter + empty battle
// v4 -> complete battle HP/PP/turn/RNG persistence
// v5 -> v4 payload followed by a 32-byte BoxRoot. Legacy roots remain empty.
// They are upgraded safely on the next successful save.
// Failure leaves output unchanged. CRC covers header (except CRC field) + payload.
DecodeResult deserialize(const uint8_t* input, size_t length, GameSave& output);
}
