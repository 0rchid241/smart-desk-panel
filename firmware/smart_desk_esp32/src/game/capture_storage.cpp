#include "capture_storage.h"
#include "box_storage.h"
#include "save_storage.h"
#include <limits>

namespace CaptureStorage {
using namespace PokemonGame;
Result attempt(GameSave& live, CaptureBall ball, BattleCaptureReport& report) {
  // Includes the B2 Indeterminate lock: even snapshot writes must stop until load.
  if (!GameSaveStorage::canWrite()) return Result::StorageError;
  if (live.state.progress.nextInstanceId == UINT32_MAX) return Result::IdExhausted;
  if (!canUseCaptureBall(live.state, ball)) return Result::Unavailable;
  const auto destination = live.state.party.count < PARTY_CAPACITY
    ? CaptureDestination::Party : CaptureDestination::Box;
  const BoxKey source{live.boxRoot.storeId, live.boxRoot.generation};
  uint32_t slot = 0;
  uint64_t generation = source.generation;
  if (destination == CaptureDestination::Box) {
    if (GameSaveStorage::validatePair(live) != GameSaveStorage::LoadResult::Loaded)
      return Result::StorageError;
    BoxStorage::Snapshot snapshot;
    if (snapshot.open(source) != BoxStorage::Result::Ok) return Result::StorageError;
    const auto empty = snapshot.findEmpty(slot);
    if (empty == BoxStorage::Result::NotFound) return Result::BoxFull;
    if (empty != BoxStorage::Result::Ok) return Result::StorageError;
    // NVS root stays authoritative. Skip at most 8 exact orphan paths;
    // never scan/adopt the newest file and never overwrite a collision.
    bool available = false;
    for (unsigned probe = 0; probe < 8; ++probe) {
      if (generation == std::numeric_limits<uint64_t>::max()) return Result::GenerationExhausted;
      ++generation;
      bool exists = false;
      if (BoxStorage::exists({source.storeId, generation}, exists) != BoxStorage::Result::Ok)
        return Result::StorageError;
      if (!exists) { available = true; break; }
    }
    if (!available) return Result::StorageError;
  }
  GameSave candidate = live;
  BattleCaptureReport captured;
  if (!attemptBattleCapture(candidate.state, ball, &captured, destination)) return Result::Unavailable;
  const BoxKey createdKey{source.storeId, generation};
  bool rollbackSafe = false;
  if (captured.captured && destination == CaptureDestination::Box) {
    // Preflight above proved this exact path absent. Record A/B non-reference
    // BEFORE mutation/NVS write; a torn NotCommitted target may no longer decode.
    rollbackSafe = GameSaveStorage::snapshotUnreferenced(createdKey);
    const BoxMutation mutation{BoxMutationKind::Insert, slot, captured.caught};
    const auto mutated = BoxStorage::mutate(source, generation, mutation);
    if (mutated != BoxStorage::Result::Ok) {
      if (rollbackSafe && mutated != BoxStorage::Result::Collision) BoxStorage::removeSnapshot(createdKey);
      return Result::StorageError;
    }
    BoxMetadata metadata;
    if (BoxStorage::validate(createdKey, metadata) != BoxStorage::Result::Ok) {
      if (rollbackSafe) BoxStorage::removeSnapshot(createdKey);
      return Result::StorageError;
    }
    candidate.boxRoot = boxRootFromMetadata(metadata);
  }
  switch (GameSaveStorage::saveDetailed(candidate)) {
    case GameSaveStorage::CommitResult::Committed:
      live = candidate; report = captured; return Result::Committed;
    case GameSaveStorage::CommitResult::NotCommitted:
      if (rollbackSafe) BoxStorage::removeSnapshot(createdKey);
      return Result::NotCommitted;
    case GameSaveStorage::CommitResult::Indeterminate: return Result::Indeterminate;
  }
  return Result::StorageError;
}
}
