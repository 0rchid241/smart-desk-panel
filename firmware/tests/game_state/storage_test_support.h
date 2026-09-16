#pragma once
#include "box_storage.h"
#include "save_storage.h"
#include "Preferences.h"
#include <esp_random.h>

inline void resetStorageFakes() {
  BoxStorage::unmount();
  FakeLittleFS::reset();
  FakeNvs::reset();
  FakeRandom::reset();
}
// Codec tests use a syntactically valid root without needing a filesystem.
inline PokemonGame::BoxRoot wireTestRoot() {
  return {0x1122334455667788ULL, 0x8877665544332211ULL, PokemonGame::BOX_CAPACITY,
          0, 0, PokemonGame::BOX_FORMAT_VERSION, 0};
}
inline bool initializeSave(PokemonGame::GameSave& save) {
  return GameSaveStorage::initialize(save) == GameSaveStorage::CommitResult::Committed;
}
