#pragma once
#include "game_state.h"
#include "pokemon_record_codec.h"

namespace PokemonGame {
constexpr uint16_t SAVE_VERSION = 4;
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
constexpr size_t SAVE_MAX_SIZE = SAVE_V3_MAX_SIZE + BATTLE_RECORD_SIZE;
struct GameSave {
  uint16_t saveVersion = SAVE_VERSION;
  uint32_t sequence = 0;
  GameState state;
};
enum class DecodeResult { Ok, Invalid, UnsupportedVersion };
// Fixed little-endian wire format, independent of compiler padding or Arduino.
bool serialize(const GameSave& save, uint8_t* output, size_t capacity, size_t& written);
// Older saves remain readable:
// v1 -> Idle exploration + empty encounter
// v2 -> existing exploration + empty encounter
// v3 -> existing exploration/encounter + empty battle
// v4 -> complete battle HP/PP/turn/RNG persistence
// They are upgraded safely on the next successful save.
// Failure leaves output unchanged. CRC covers header (except CRC field) + payload.
DecodeResult deserialize(const uint8_t* input, size_t length, GameSave& output);
}
