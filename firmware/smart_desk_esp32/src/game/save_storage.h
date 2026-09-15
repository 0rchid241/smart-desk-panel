#pragma once
#include "save_data.h"

// ESP32 adapter. Core types and the wire codec do not include Preferences.
namespace GameSaveStorage {
enum class LoadResult { Loaded, Missing, Invalid, StorageError, UnsupportedVersion };
LoadResult load(PokemonGame::GameSave& save);
// Call load first. Only a verified successful write advances save.sequence.
bool save(PokemonGame::GameSave& save);
}
