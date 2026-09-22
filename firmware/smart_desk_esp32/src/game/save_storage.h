#pragma once
#include "save_data.h"

// ESP32 adapter. Core types and the wire codec do not include Preferences.
namespace GameSaveStorage {
enum class LoadResult { Loaded, Missing, Invalid, StorageError, UnsupportedVersion, RecoveryRequired };
enum class CommitResult { Committed, NotCommitted, Indeterminate };
enum class GcStatus { Clean, SkippedUnsafe, IoError, Partial };
struct GcReport {
  GcStatus status = GcStatus::SkippedUnsafe;
  uint32_t removed = 0, kept = 0, deferred = 0;
};
// Serialized maintenance only; rereads BOTH NVS slots. Never affects CommitResult.
// At most 16 unreferenced keys per call; overflow is deferred, with bounded RAM.
GcReport collectBoxGarbage(const PokemonGame::BoxRoot* liveRoot = nullptr);
GcReport lastBoxGc();
// Pre-write proof for exact rollback cleanup: known A/B + active root do not refer
// to key. Caller must also prove the destination was absent, own its creation,
// and only delete before NVS write or after a definite NotCommitted result.
bool snapshotUnreferenced(PokemonGame::BoxKey key);
LoadResult load(PokemonGame::GameSave& save);
// Validates the Box snapshot and Party/Box ownership without mutating storage.
LoadResult validatePair(const PokemonGame::GameSave& save);
// Explicit Missing/legacy path. Creates an empty Box before the v5 NVS commit.
CommitResult initialize(PokemonGame::GameSave& save);
CommitResult saveDetailed(PokemonGame::GameSave& save);
// Read-only gate for coordinators before creating files or consuming RNG.
bool canWrite();
// Call load first. Only a verified successful write advances save.sequence.
// Indeterminate blocks all writes until load() resolves persistent A/B state.
bool save(PokemonGame::GameSave& save);
}
