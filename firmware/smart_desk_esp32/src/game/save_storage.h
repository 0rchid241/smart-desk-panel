#pragma once
#include "save_data.h"

// ESP32 adapter. Core types and the wire codec do not include Preferences.
namespace GameSaveStorage {
enum class LoadResult { Loaded, Missing, Invalid, StorageError, UnsupportedVersion, RecoveryRequired };
enum class CommitResult { Committed, NotCommitted, Indeterminate };
LoadResult load(PokemonGame::GameSave& save);
// Validates the Box snapshot and Party/Box ownership without mutating storage.
LoadResult validatePair(const PokemonGame::GameSave& save);
// Explicit Missing/legacy path. Creates an empty Box before the v5 NVS commit.
CommitResult initialize(PokemonGame::GameSave& save);
CommitResult saveDetailed(PokemonGame::GameSave& save);
// Call load first. Only a verified successful write advances save.sequence.
// Indeterminate blocks all writes until load() resolves persistent A/B state.
bool save(PokemonGame::GameSave& save);
}
