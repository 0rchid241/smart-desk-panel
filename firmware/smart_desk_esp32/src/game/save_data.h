#pragma once
#include "game_state.h"

namespace PokemonGame {
constexpr uint16_t SAVE_VERSION = 1;
constexpr size_t SAVE_HEADER_SIZE = 18;
constexpr size_t POKEMON_RECORD_SIZE = 24;
constexpr size_t SAVE_MAX_SIZE = SAVE_HEADER_SIZE + 14 + 1 +
                               PARTY_CAPACITY * POKEMON_RECORD_SIZE + 3 * POKEDEX_BYTES;
struct GameSave {
  uint16_t saveVersion = SAVE_VERSION;
  uint32_t sequence = 0;
  GameState state;
};
enum class DecodeResult { Ok, Invalid, UnsupportedVersion };
// Fixed little-endian wire format, independent of compiler padding or Arduino.
bool serialize(const GameSave& save, uint8_t* output, size_t capacity, size_t& written);
// Failure leaves output unchanged. CRC covers header (except CRC field) + payload.
DecodeResult deserialize(const uint8_t* input, size_t length, GameSave& output);
}
