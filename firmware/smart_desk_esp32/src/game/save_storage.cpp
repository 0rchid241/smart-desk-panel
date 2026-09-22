#include "save_storage.h"
#include "box_storage.h"
#include <Preferences.h>
#include <nvs.h>
#include <esp_random.h>
#include <cstring>
#include <limits>

namespace GameSaveStorage {
namespace {
using namespace PokemonGame;
Preferences storage;
const char* const slots[] = {"save_a", "save_b"};
int activeSlot = -1;
uint32_t sequence = 0, activeCrc = 0;
uint16_t activeVersion = 0;
bool writable = false, indeterminate = false, mayFormat = false, mayInitialize = false;
BoxKey activeBoxKey;
GcReport gcReport;
struct SlotInfo {
  bool modern = false;
  bool unknown = false;
  uint32_t crc = 0;
};
LoadResult readSlot(int slot, GameSave& output, SlotInfo& info) {
  // Preferences::isKey/getBytesLength collapse NVS errors into false/zero.
  // First-use formatting must require a proven NOT_FOUND, never an I/O error.
  nvs_handle_t handle = 0;
  if (nvs_open("pokemon_g1", NVS_READONLY, &handle) != ESP_OK) return LoadResult::StorageError;
  size_t length = 0;
  auto error = nvs_get_blob(handle, slots[slot], nullptr, &length);
  if (error != ESP_OK || length == 0 || length > SAVE_MAX_SIZE) {
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return LoadResult::Missing;
    info.unknown = true;
    if (error != ESP_OK) return LoadResult::StorageError;
    return length > SAVE_MAX_SIZE ? LoadResult::UnsupportedVersion : LoadResult::Invalid;
  }
  uint8_t bytes[SAVE_MAX_SIZE];
  const size_t expectedLength = length;
  error = nvs_get_blob(handle, slots[slot], bytes, &length);
  nvs_close(handle);
  if (error != ESP_OK || length != expectedLength) {
    info.unknown = true; return LoadResult::StorageError;
  }
  if (length >= 6 && std::memcmp(bytes, "PKDG", 4) == 0) {
    const uint16_t version = static_cast<uint16_t>(bytes[4] | (bytes[5] << 8));
    info.modern = version >= 5;
    // Even a damaged future header must not authorize format or replacement.
    if (version > SAVE_VERSION) return LoadResult::UnsupportedVersion;
  } else info.unknown = true;
  if (length >= SAVE_HEADER_SIZE)
    for (unsigned i = 0; i < 4; ++i) info.crc |= static_cast<uint32_t>(bytes[14 + i]) << (8 * i);
  switch (deserialize(bytes, length, output)) {
    case DecodeResult::Ok: return LoadResult::Loaded;
    case DecodeResult::UnsupportedVersion: return LoadResult::UnsupportedVersion;
    default: info.unknown = true; return LoadResult::Invalid;
  }
}
CommitResult uncertain() {
  writable = false; mayFormat = false; mayInitialize = false; indeterminate = true;
  return CommitResult::Indeterminate;
}
bool isIo(LoadResult result) {
  return result == LoadResult::StorageError || result == LoadResult::UnsupportedVersion;
}
}

namespace {
bool sameKey(BoxKey a, BoxKey b) { return a.storeId == b.storeId && a.generation == b.generation; }
struct ProtectedRoots {
  BoxKey keys[4] = {};
  size_t count = 0;
  bool contains(BoxKey key) const {
    for (size_t i = 0; i < count; ++i) if (sameKey(keys[i], key)) return true;
    return false;
  }
  void add(BoxKey key) { if (key.storeId && !contains(key)) keys[count++] = key; }
};
bool readProtectedRoots(ProtectedRoots& roots, const BoxRoot* liveRoot) {
  if (indeterminate || !writable) return false;
  for (int slot = 0; slot < 2; ++slot) {
    GameSave save; SlotInfo info;
    const auto result = readSlot(slot, save, info);
    if (result == LoadResult::Missing) continue; // Proven NVS NOT_FOUND only.
    if (result != LoadResult::Loaded) return false;
    // Do not discard a decoded root just because validatePair would reject its file.
    if (save.saveVersion == SAVE_VERSION) roots.add({save.boxRoot.storeId, save.boxRoot.generation});
  }
  roots.add(activeBoxKey);
  if (liveRoot) {
    if (!isValidBoxRoot(*liveRoot)) return false;
    roots.add({liveRoot->storeId, liveRoot->generation});
  }
  return true;
}
struct GcScan {
  ProtectedRoots roots;
  BoxKey candidates[16] = {};
  size_t count = 0;
  uint32_t kept = 0, deferred = 0;
};
void considerSnapshot(BoxKey key, void* context) {
  auto& scan = *static_cast<GcScan*>(context);
  if (scan.roots.contains(key)) ++scan.kept;
  else if (scan.count < 16) scan.candidates[scan.count++] = key;
  else ++scan.deferred;
}
}
bool snapshotUnreferenced(PokemonGame::BoxKey key) {
  ProtectedRoots roots;
  return key.storeId && key.generation && readProtectedRoots(roots, nullptr) && !roots.contains(key);
}
GcReport lastBoxGc() { return gcReport; }
GcReport collectBoxGarbage(const PokemonGame::BoxRoot* liveRoot) {
  GcReport report;
  GcScan scan;
  if (!readProtectedRoots(scan.roots, liveRoot)) return gcReport = report;
  if (BoxStorage::mount() != BoxStorage::Result::Ok ||
      BoxStorage::visitSnapshotKeys(considerSnapshot, &scan) != BoxStorage::Result::Ok) {
    report.status = GcStatus::IoError;
    return gcReport = report; // No deletion before the WHOLE scan and close succeed.
  }
  report.kept = scan.kept; report.deferred = scan.deferred;
  report.status = scan.deferred ? GcStatus::Partial : GcStatus::Clean;
  for (size_t i = 0; i < scan.count; ++i) {
    const BoxKey key = scan.candidates[i];
    if (scan.roots.contains(key)) continue; // Never pass a protected key to remove.
    if (BoxStorage::removeSnapshot(key) != BoxStorage::Result::Ok) {
      report.status = report.removed ? GcStatus::Partial : GcStatus::IoError;
      break; // Already completed deletions remain safe; next maintenance can retry.
    }
    ++report.removed;
  }
  return gcReport = report;
}

LoadResult validatePair(const PokemonGame::GameSave& save) {
  using namespace PokemonGame;
  if (!isValidState(save.state)) return LoadResult::Invalid;
  if (save.saveVersion >= 1 && save.saveVersion <= 4) return LoadResult::Loaded;
  if (save.saveVersion != SAVE_VERSION || !isValidBoxRoot(save.boxRoot)) return LoadResult::Invalid;
  if (BoxStorage::mount() != BoxStorage::Result::Ok) return LoadResult::StorageError;
  BoxStorage::Snapshot snapshot;
  const auto& root = save.boxRoot;
  const auto opened = snapshot.open({root.storeId, root.generation});
  if (opened == BoxStorage::Result::IoError || opened == BoxStorage::Result::NotMounted)
    return LoadResult::StorageError;
  if (opened != BoxStorage::Result::Ok) return LoadResult::RecoveryRequired;
  const auto& metadata = snapshot.metadata();
  if (metadata.key.storeId != root.storeId || metadata.key.generation != root.generation ||
      metadata.occupiedCount != root.occupiedCount || metadata.crc32 != root.snapshotCrc32)
    return LoadResult::RecoveryRequired;
  for (uint32_t slot = 0; slot < BOX_CAPACITY; ++slot) {
    bool occupied = false;
    if (snapshot.occupied(slot, occupied) != BoxStorage::Result::Ok) return LoadResult::StorageError;
    if (!occupied) continue;
    PokemonInstance pokemon;
    const auto result = snapshot.readSlot(slot, pokemon);
    if (result == BoxStorage::Result::IoError) return LoadResult::StorageError;
    if (result != BoxStorage::Result::Ok || !pokemon.instanceId ||
        pokemon.instanceId >= save.state.progress.nextInstanceId) return LoadResult::RecoveryRequired;
    for (uint8_t i = 0; i < save.state.party.count; ++i)
      if (save.state.party.members[i].instanceId == pokemon.instanceId) return LoadResult::RecoveryRequired;
    const auto& dex = save.state.pokedex;
    if (!dexContains(dex.seen, pokemon.speciesId) || !dexContains(dex.caught, pokemon.speciesId) ||
        (pokemon.shiny && !dexContains(dex.shinyCaught, pokemon.speciesId)))
      return LoadResult::RecoveryRequired;
  }
  return LoadResult::Loaded;
}

LoadResult load(PokemonGame::GameSave& save) {
  storage.end();
  BoxStorage::unmount();
  writable = indeterminate = mayFormat = mayInitialize = false;
  activeSlot = -1; sequence = activeCrc = 0; activeVersion = 0;
  activeBoxKey = {}; gcReport = {};
  if (!storage.begin("pokemon_g1", false)) return LoadResult::StorageError;
  GameSave a, b;
  SlotInfo ia, ib;
  LoadResult ra = readSlot(0, a, ia), rb = readSlot(1, b, ib);
  if (ra == LoadResult::UnsupportedVersion || rb == LoadResult::UnsupportedVersion)
    return LoadResult::UnsupportedVersion;
  if (ra == LoadResult::StorageError || rb == LoadResult::StorageError) return LoadResult::StorageError;
  const bool missing = ra == LoadResult::Missing && rb == LoadResult::Missing;
  if (ra == LoadResult::Loaded) ra = validatePair(a);
  if (rb == LoadResult::Loaded) rb = validatePair(b);
  // An unreadable device is not proof of corruption: do not roll back and write.
  if (isIo(ra) || isIo(rb)) return LoadResult::StorageError;
  if (ra == LoadResult::Loaded || rb == LoadResult::Loaded) {
    activeSlot = rb == LoadResult::Loaded && (ra != LoadResult::Loaded || b.sequence > a.sequence) ? 1 : 0;
    save = activeSlot == 0 ? a : b;
    sequence = save.sequence; activeVersion = save.saveVersion;
    activeCrc = activeSlot == 0 ? ia.crc : ib.crc;
    writable = true;
    mayInitialize = activeVersion < SAVE_VERSION;
    mayFormat = mayInitialize && !ia.modern && !ib.modern && !ia.unknown && !ib.unknown;
    if (activeVersion == SAVE_VERSION) {
      activeBoxKey = {save.boxRoot.storeId, save.boxRoot.generation};
      collectBoxGarbage(&save.boxRoot);
    }
    return LoadResult::Loaded;
  }
  if (missing) {
    writable = mayInitialize = mayFormat = true;
    return LoadResult::Missing;
  }
  // Never replace corrupt saves with a fresh game, including unidentifiable headers.
  return ia.modern || ib.modern || ra == LoadResult::RecoveryRequired || rb == LoadResult::RecoveryRequired
    ? LoadResult::RecoveryRequired : LoadResult::Invalid;
}

CommitResult saveDetailed(PokemonGame::GameSave& save) {
  if (indeterminate) return CommitResult::Indeterminate;
  if (!writable || sequence == std::numeric_limits<uint32_t>::max() ||
      save.sequence != sequence || save.saveVersion != SAVE_VERSION) return CommitResult::NotCommitted;
  GameSave candidate = save;
  candidate.sequence = sequence + 1;
  if (validatePair(candidate) != LoadResult::Loaded) return CommitResult::NotCommitted;
  uint8_t bytes[SAVE_MAX_SIZE], verified[SAVE_MAX_SIZE];
  size_t length = 0;
  if (!serialize(candidate, bytes, sizeof(bytes), length)) return CommitResult::NotCommitted;
  const int target = activeSlot == 0 ? 1 : 0;
  // After any attempted v5 write, never format again in this running session.
  mayFormat = false;
  storage.putBytes(slots[target], bytes, length);
  GameSave stored;
  SlotInfo info;
  const auto result = readSlot(target, stored, info);
  if (isIo(result)) return uncertain();
  if (result == LoadResult::Loaded) {
    const auto pair = validatePair(stored);
    if (isIo(pair)) return uncertain();
    size_t size = 0;
    if (pair == LoadResult::Loaded && serialize(stored, verified, sizeof(verified), size) &&
        size == length && std::memcmp(bytes, verified, length) == 0) {
      activeSlot = target; sequence = candidate.sequence; activeCrc = info.crc;
      activeVersion = SAVE_VERSION; mayInitialize = false;
      save = candidate;
      activeBoxKey = {save.boxRoot.storeId, save.boxRoot.generation};
      collectBoxGarbage(&save.boxRoot); // Best-effort; cannot change this confirmed commit.
      return CommitResult::Committed;
    }
    // A valid unexpected newer main cannot be safely retried with this sequence.
    if (stored.sequence >= candidate.sequence) return uncertain();
  }
  if (activeSlot >= 0) {
    GameSave previous;
    SlotInfo previousInfo;
    if (readSlot(activeSlot, previous, previousInfo) != LoadResult::Loaded ||
        previous.sequence != sequence || previousInfo.crc != activeCrc ||
        validatePair(previous) != LoadResult::Loaded) return uncertain();
  }
  return CommitResult::NotCommitted;
}

CommitResult initialize(PokemonGame::GameSave& save) {
  if (indeterminate) return CommitResult::Indeterminate;
  if (!writable || save.sequence != sequence || !isValidState(save.state)) return CommitResult::NotCommitted;
  if (!mayInitialize) {
    // Already migrated: no extra snapshot and no duplicate NVS commit.
    uint8_t bytes[SAVE_MAX_SIZE]; size_t length = 0;
    if (activeVersion != SAVE_VERSION || save.saveVersion != SAVE_VERSION ||
        validatePair(save) != LoadResult::Loaded || !serialize(save, bytes, sizeof(bytes), length))
      return CommitResult::NotCommitted;
    uint32_t crc = 0;
    for (unsigned i = 0; i < 4; ++i) crc |= static_cast<uint32_t>(bytes[14 + i]) << (8 * i);
    return crc == activeCrc ? CommitResult::Committed : CommitResult::NotCommitted;
  }
  if (BoxStorage::mount() != BoxStorage::Result::Ok) {
    if (!mayFormat || BoxStorage::initializeFilesystem() != BoxStorage::Result::Ok)
      return CommitResult::NotCommitted;
  }
  BoxKey key;
  key.generation = 1;
  bool created = false;
  for (unsigned attempt = 0; attempt < 8; ++attempt) {
    const uint64_t high = esp_random();
    key.storeId = (high << 32) | esp_random();
    if (!key.storeId) continue;
    const auto result = BoxStorage::createEmpty(key);
    if (result == BoxStorage::Result::Collision) continue;
    if (result != BoxStorage::Result::Ok) return CommitResult::NotCommitted;
    created = true; break;
  }
  if (!created) return CommitResult::NotCommitted;
  BoxMetadata metadata;
  if (BoxStorage::validate(key, metadata) != BoxStorage::Result::Ok) return CommitResult::NotCommitted;
  GameSave candidate = save;
  candidate.saveVersion = SAVE_VERSION;
  candidate.boxRoot = boxRootFromMetadata(metadata);
  const auto result = saveDetailed(candidate);
  if (result == CommitResult::Committed) save = candidate;
  // Failed initialization files remain for later proven-safe global GC; uncertainty preserves them.
  return result;
}
bool canWrite() { return writable && !indeterminate; }
bool save(PokemonGame::GameSave& save) { return saveDetailed(save) == CommitResult::Committed; }
}
