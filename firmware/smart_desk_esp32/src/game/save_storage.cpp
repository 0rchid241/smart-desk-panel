#include "save_storage.h"
#include <Preferences.h>
#include <cstring>
#include <limits>

namespace GameSaveStorage {
namespace {
using namespace PokemonGame;
Preferences storage;
const char* const slots[] = {"save_a", "save_b"};
int activeSlot = -1;
uint32_t sequence = 0;
bool writable = false;

LoadResult readSlot(int slot, GameSave& output) {
  if (!storage.isKey(slots[slot])) return LoadResult::Missing;
  const size_t length = storage.getBytesLength(slots[slot]);
  if (length == 0) return LoadResult::Invalid;
  // A larger record may belong to a future format: preserve it on downgrade.
  if (length > SAVE_MAX_SIZE) return LoadResult::UnsupportedVersion;
  uint8_t bytes[SAVE_MAX_SIZE];
  if (storage.getBytes(slots[slot], bytes, length) != length) return LoadResult::StorageError;
  switch (deserialize(bytes, length, output)) {
    case DecodeResult::Ok: return LoadResult::Loaded;
    case DecodeResult::UnsupportedVersion: return LoadResult::UnsupportedVersion;
    default: return LoadResult::Invalid;
  }
}
}

LoadResult load(PokemonGame::GameSave& save) {
  storage.end();
  writable = false;
  activeSlot = -1;
  sequence = 0;
  if (!storage.begin("pokemon_g1", false)) return LoadResult::StorageError;
  PokemonGame::GameSave a, b;
  const LoadResult ra = readSlot(0, a), rb = readSlot(1, b);
  // Do not replace data from a newer firmware or an unreadable storage device.
  if (ra == LoadResult::UnsupportedVersion || rb == LoadResult::UnsupportedVersion)
    return LoadResult::UnsupportedVersion;
  if (ra == LoadResult::StorageError || rb == LoadResult::StorageError)
    return LoadResult::StorageError;
  writable = true;
  if (ra == LoadResult::Loaded || rb == LoadResult::Loaded) {
    activeSlot = (rb == LoadResult::Loaded &&
                  (ra != LoadResult::Loaded || b.sequence > a.sequence)) ? 1 : 0;
    save = activeSlot == 0 ? a : b;
    sequence = save.sequence;
    return LoadResult::Loaded;
  }
  return ra == LoadResult::Missing && rb == LoadResult::Missing
           ? LoadResult::Missing : LoadResult::Invalid;
}

bool save(PokemonGame::GameSave& save) {
  using namespace PokemonGame;
  if (!writable || sequence == std::numeric_limits<uint32_t>::max()) return false;
  GameSave candidate = save;
  candidate.saveVersion = SAVE_VERSION;
  candidate.sequence = sequence + 1;
  uint8_t bytes[SAVE_MAX_SIZE], verified[SAVE_MAX_SIZE];
  size_t length = 0;
  if (!serialize(candidate, bytes, sizeof(bytes), length)) return false;
  const int target = activeSlot == 0 ? 1 : 0;
  if (storage.putBytes(slots[target], bytes, length) != length ||
      storage.getBytesLength(slots[target]) != length ||
      storage.getBytes(slots[target], verified, length) != length ||
      std::memcmp(bytes, verified, length) != 0) return false;
  activeSlot = target;
  sequence = candidate.sequence;
  save.sequence = sequence;
  save.saveVersion = candidate.saveVersion;
  return true;
}
}
