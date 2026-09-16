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
  if (captured.captured && destination == CaptureDestination::Box) {
    const BoxMutation mutation{BoxMutationKind::Insert, slot, captured.caught};
    if (BoxStorage::mutate(source, generation, mutation) != BoxStorage::Result::Ok)
      return Result::StorageError;
    BoxMetadata metadata;
    if (BoxStorage::validate({source.storeId, generation}, metadata) != BoxStorage::Result::Ok)
      return Result::StorageError;
    candidate.boxRoot = boxRootFromMetadata(metadata);
  }
  // No cleanup here: failed/uncertain writes retain the exact orphan and both
  // A/B roots. A later bounded retry uses another generation from the same source.
  switch (GameSaveStorage::saveDetailed(candidate)) {
    case GameSaveStorage::CommitResult::Committed:
      live = candidate; report = captured; return Result::Committed;
    case GameSaveStorage::CommitResult::NotCommitted: return Result::NotCommitted;
    case GameSaveStorage::CommitResult::Indeterminate: return Result::Indeterminate;
  }
  return Result::StorageError;
}
}
