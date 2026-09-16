#include "save_wire_fixture.h"
#include "save_v4_golden.h"
#include <cassert>
#include <cstdio>
#include <cstring>

void saveWireTests() {
  using namespace PokemonGame;
  static_assert(SAVE_VERSION == 4 && SAVE_MAX_SIZE == 554 && POKEMON_RECORD_SIZE == 24,
                "B1 preserves v4");
  uint8_t bytes[SAVE_MAX_SIZE];
  size_t written = 0;
  assert(serialize(legacyWireInput(), bytes, sizeof(bytes), written));
  assert(written == sizeof(SAVE_V4_GOLDEN));
  assert(std::memcmp(bytes, SAVE_V4_GOLDEN, written) == 0);
  GameSave decoded;
  assert(deserialize(SAVE_V4_GOLDEN, sizeof(SAVE_V4_GOLDEN), decoded) == DecodeResult::Ok);
  assert(serialize(decoded, bytes, sizeof(bytes), written));
  assert(std::memcmp(bytes, SAVE_V4_GOLDEN, written) == 0);
  std::puts("PASS v4 golden: all 554 bytes (including CRC) match pre-B1 serializer at 2d699e2");
}
