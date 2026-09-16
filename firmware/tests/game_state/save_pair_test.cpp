#include "storage_test_support.h"
#include "save_v4_golden.h"
#include "save_wire_fixture.h"
#include "battle.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace PokemonGame;
using namespace GameSaveStorage;
std::vector<uint8_t> encode(const GameSave& save);
void updateCrc(std::vector<uint8_t>& bytes);
namespace {
std::string boxPath(const BoxRoot& root) {
  char path[BoxStorage::PATH_SIZE];
  assert(BoxStorage::snapshotPath({root.storeId, root.generation}, path, sizeof(path)));
  return path;
}
BoxRoot rootFor(BoxKey key) {
  BoxMetadata metadata;
  assert(BoxStorage::validate(key, metadata) == BoxStorage::Result::Ok);
  return boxRootFromMetadata(metadata);
}
GameSave fresh() {
  resetStorageFakes();
  GameSave save;
  assert(load(save) == LoadResult::Missing);
  save.state = createNewGame();
  assert(initialize(save) == CommitResult::Committed);
  assert(save.saveVersion == 5 && save.sequence == 1 && save.boxRoot.generation == 1);
  assert(save.boxRoot.capacity == 2048 && save.boxRoot.occupiedCount == 0);
  assert(save.boxRoot.boxFormatVersion == 1 && validatePair(save) == LoadResult::Loaded);
  return save;
}
std::vector<uint8_t> asV4(const GameSave& save) {
  auto bytes = encode(save);
  bytes.resize(bytes.size() - BOX_ROOT_RECORD_SIZE); bytes[4] = 4;
  const auto length = static_cast<uint32_t>(bytes.size() - SAVE_HEADER_SIZE);
  for (unsigned i = 0; i < 4; ++i) bytes[10 + i] = static_cast<uint8_t>(length >> (8 * i));
  updateCrc(bytes);
  return bytes;
}
void equalState(GameSave a, GameSave b) {
  a.sequence = b.sequence = 0;
  a.boxRoot = b.boxRoot = wireTestRoot();
  assert(encode(a) == encode(b));
}
void migration() {
  for (unsigned scenario = 0; scenario < 3; ++scenario) {
    resetStorageFakes();
    auto original = legacyWireInput();
    std::vector<uint8_t> legacy(SAVE_V4_GOLDEN, SAVE_V4_GOLDEN + sizeof(SAVE_V4_GOLDEN));
    if (scenario == 1) {
      assert(setEncounter(original.state.encounter, 19, 0, 5, Gender::Female, true));
      assert(startBattle(original.state));
      original.state.battle.turn = 17; original.state.battle.rngState = 0x12345678;
      original.state.battle.player.currentHp = 1;
      original.state.battle.opponent.currentHp = 2;
      original.state.battle.player.pp[0] = 1;
      legacy = asV4(original);
    } else if (scenario == 2) {
      assert(startExploration(original.state.exploration, TEST_REGION_ID, 1800000000ULL, 15));
      assert(updateExploration(original.state.exploration, 1800000015ULL));
      assert(setEncounter(original.state.encounter, 16, 0, 4, Gender::Female, false));
      legacy = asV4(original);
    }
    FakeNvs::data["pokemon_g1/save_a"] = legacy;
    FakeLittleFS::needsFormat = true;
    GameSave loaded;
    assert(load(loaded) == LoadResult::Loaded && loaded.saveVersion == 4);
    assert(loaded.boxRoot.storeId == 0);
    assert(initialize(loaded) == CommitResult::Committed);
    assert(loaded.sequence == original.sequence + 1 && loaded.saveVersion == 5);
    assert(loaded.boxRoot.occupiedCount == 0 && loaded.boxRoot.generation == 1);
    equalState(original, loaded);
    assert(FakeLittleFS::formats == 1 && !FakeLittleFS::formatRequested);
    assert(FakeNvs::data.at("pokemon_g1/save_a") == legacy);
    const auto files = FakeLittleFS::files.size(); const auto writes = FakeNvs::writes;
    assert(initialize(loaded) == CommitResult::Committed);
    assert(FakeLittleFS::files.size() == files && FakeNvs::writes == writes);
    auto modified = loaded; ++modified.state.progress.playTimeSeconds;
    assert(initialize(modified) == CommitResult::NotCommitted);
    GameSave reboot;
    assert(load(reboot) == LoadResult::Loaded && encode(reboot) == encode(loaded));
    assert(initialize(reboot) == CommitResult::Committed && FakeNvs::writes == writes);
  }
}
void initializationFailures() {
  resetStorageFakes();
  GameSave unreadable; unreadable.state = createNewGame();
  FakeNvs::failRead = true;
  assert(load(unreadable) == LoadResult::StorageError); // NOT a first install.
  assert(initialize(unreadable) == CommitResult::NotCommitted);
  assert(FakeLittleFS::formats == 0 && FakeNvs::writes == 0);
  for (unsigned scenario = 0; scenario < 4; ++scenario) {
    resetStorageFakes();
    GameSave save; save.state = createNewGame();
    assert(load(save) == LoadResult::Missing);
    if (scenario == 0) { FakeLittleFS::needsFormat = true; FakeLittleFS::failFormat = true; }
    if (scenario == 1) FakeLittleFS::failMount = true;
    if (scenario == 2) FakeLittleFS::failOpen = true;
    if (scenario == 3) FakeLittleFS::writeBudget = 100;
    assert(initialize(save) == CommitResult::NotCommitted);
    assert(FakeNvs::data.empty() && FakeNvs::writes == 0 && save.boxRoot.storeId == 0);
  }
  resetStorageFakes();
  FakeLittleFS::needsFormat = true;
  GameSave save; save.state = createNewGame();
  assert(load(save) == LoadResult::Missing);
  assert(initialize(save) == CommitResult::Committed && FakeLittleFS::formats == 1);
  const auto original = encode(save);
  assert(load(save) == LoadResult::Loaded && encode(save) == original);

  // Format is forbidden when any modern/future/unidentified record exists.
  for (unsigned scenario = 0; scenario < 4; ++scenario) {
    auto good = fresh();
    FakeNvs::data["pokemon_g1/save_b"] = std::vector<uint8_t>(SAVE_V4_GOLDEN, SAVE_V4_GOLDEN + 554);
    if (scenario == 1) { FakeNvs::data["pokemon_g1/save_a"][4] = 6; }
    if (scenario == 2) { FakeNvs::data["pokemon_g1/save_a"].back() ^= 1; }
    if (scenario == 3) { FakeNvs::data["pokemon_g1/save_a"][0] ^= 1; }
    const auto nvs = FakeNvs::data;
    FakeLittleFS::failMount = true;
    const auto result = load(good);
    if (scenario == 0) assert(result == LoadResult::StorageError);
    if (scenario == 1) assert(result == LoadResult::UnsupportedVersion);
    assert(initialize(good) != CommitResult::Committed);
    assert(FakeLittleFS::formats == 0 && FakeNvs::data == nvs);
  }
  // Random zero/collision is retried, without clock dependence or overwrites.
  resetStorageFakes();
  assert(BoxStorage::mount() == BoxStorage::Result::Ok);
  assert(BoxStorage::createEmpty({(uint64_t(1) << 32) | 2, 1}) == BoxStorage::Result::Ok);
  FakeRandom::values = {0, 0, 1, 2, 3, 4};
  save = {}; save.state = createNewGame();
  assert(load(save) == LoadResult::Missing);
  assert(initialize(save) == CommitResult::Committed);
  assert(save.boxRoot.storeId == ((uint64_t(3) << 32) | 4) && FakeLittleFS::files.size() == 2);
}
void pairRecovery() {
  auto previous = fresh();
  const auto key = BoxKey{previous.boxRoot.storeId, 2};
  assert(BoxStorage::createEmpty(key) == BoxStorage::Result::Ok);
  auto latest = previous; latest.boxRoot = rootFor(key);
  latest.state.progress.masterBallCount = 3;
  assert(saveDetailed(latest) == CommitResult::Committed);
  const auto latestBytes = FakeNvs::data.at("pokemon_g1/save_b");
  const auto snapshotBytes = *FakeLittleFS::files.at(boxPath(latest.boxRoot));
  GameSave loaded;
  assert(load(loaded) == LoadResult::Loaded && encode(loaded) == encode(latest));
  for (unsigned scenario = 0; scenario < 6; ++scenario) {
    FakeNvs::data["pokemon_g1/save_b"] = latestBytes;
    FakeLittleFS::files[boxPath(latest.boxRoot)] = std::make_shared<FakeLittleFS::Bytes>(snapshotBytes);
    auto broken = latest;
    if (scenario == 0) FakeLittleFS::files.erase(boxPath(latest.boxRoot));
    if (scenario == 1) FakeLittleFS::files.at(boxPath(latest.boxRoot))->back() ^= 1;
    if (scenario == 2) { broken.boxRoot.snapshotCrc32 ^= 1; }
    if (scenario == 3) { ++broken.boxRoot.storeId; }
    if (scenario == 4) { ++broken.boxRoot.generation; }
    if (scenario == 5) { ++broken.boxRoot.occupiedCount; }
    if (scenario >= 2) FakeNvs::data["pokemon_g1/save_b"] = encode(broken);
    assert(load(loaded) == LoadResult::Loaded && encode(loaded) == encode(previous));
  }
  FakeLittleFS::files.clear();
  const auto nvs = FakeNvs::data;
  assert(load(loaded) == LoadResult::RecoveryRequired);
  assert(!save(loaded) && initialize(loaded) == CommitResult::NotCommitted && FakeNvs::data == nvs);
  assert(FakeLittleFS::files.empty() && FakeLittleFS::formats == 0);
}
void ownershipValidation() {
  auto save = fresh();
  auto pokemon = save.state.party.members[0];
  pokemon.instanceId = 2; pokemon.speciesId = 19; pokemon.currentHp = 3; pokemon.shiny = true;
  BoxMutation mutation{BoxMutationKind::Insert, 100, pokemon};
  const auto source = BoxKey{save.boxRoot.storeId, 1};
  assert(BoxStorage::mutate(source, 2, mutation) == BoxStorage::Result::Ok);
  save.boxRoot = rootFor({source.storeId, 2});
  save.state.progress.nextInstanceId = 3;
  registerCaught(save.state.pokedex, 19, true);
  assert(validatePair(save) == LoadResult::Loaded);
  auto invalid = save;
  invalid.state.progress.nextInstanceId = 2;
  assert(validatePair(invalid) == LoadResult::RecoveryRequired);
  invalid = save; invalid.state.pokedex = createNewGame().pokedex;
  assert(validatePair(invalid) == LoadResult::RecoveryRequired);
  invalid = save; invalid.state.pokedex.shinyCaught[2] = 0;
  assert(validatePair(invalid) == LoadResult::RecoveryRequired);
  mutation.pokemon.instanceId = 1;
  assert(BoxStorage::mutate(source, 3, mutation) == BoxStorage::Result::Ok);
  invalid = save; invalid.boxRoot = rootFor({source.storeId, 3});
  assert(validatePair(invalid) == LoadResult::RecoveryRequired);
  mutation.pokemon.instanceId = 2; mutation.pokemon.shiny = false;
  assert(BoxStorage::mutate(source, 4, mutation) == BoxStorage::Result::Ok);
  save.boxRoot = rootFor({source.storeId, 4});
  assert(validatePair(save) == LoadResult::Loaded); // Historical shinyCaught remains valid.
  assert(saveDetailed(save) == CommitResult::Committed);
  GameSave loaded; assert(load(loaded) == LoadResult::Loaded && encode(loaded) == encode(save));
}
void commitsAndMigrationCuts() {
  auto save = fresh();
  const auto root = save.boxRoot;
  const auto fileCount = FakeLittleFS::files.size();
  const auto boxBytes = *FakeLittleFS::files.at(boxPath(root));
  auto candidate = save; ++candidate.state.progress.playTimeSeconds;
  FakeNvs::rejectWrite = true;
  assert(saveDetailed(candidate) == CommitResult::NotCommitted && candidate.sequence == save.sequence);
  FakeNvs::rejectWrite = false; FakeNvs::partialWrite = true;
  assert(saveDetailed(candidate) == CommitResult::NotCommitted);
  FakeNvs::partialWrite = false;
  assert(saveDetailed(candidate) == CommitResult::Committed);
  assert(candidate.boxRoot.generation == root.generation && FakeLittleFS::files.size() == fileCount);
  assert(*FakeLittleFS::files.at(boxPath(root)) == boxBytes);
  const auto before = candidate;
  ++candidate.state.progress.playTimeSeconds;
  FakeNvs::failReadAfterWrite = true;
  assert(saveDetailed(candidate) == CommitResult::Indeterminate);
  assert(candidate.sequence == before.sequence);
  const auto writes = FakeNvs::writes;
  FakeNvs::failRead = FakeNvs::failReadAfterWrite = false;
  assert(saveDetailed(candidate) == CommitResult::Indeterminate && !GameSaveStorage::save(candidate));
  assert(initialize(candidate) == CommitResult::Indeterminate && FakeNvs::writes == writes);
  GameSave reboot;
  assert(load(reboot) == LoadResult::Loaded && reboot.sequence == before.sequence + 1);
  assert(reboot.state.progress.playTimeSeconds == candidate.state.progress.playTimeSeconds);
  assert(saveDetailed(reboot) == CommitResult::Committed);

  for (unsigned scenario = 0; scenario < 4; ++scenario) {
    resetStorageFakes();
    const std::vector<uint8_t> legacy(SAVE_V4_GOLDEN, SAVE_V4_GOLDEN + 554);
    FakeNvs::data["pokemon_g1/save_a"] = legacy;
    GameSave loaded; assert(load(loaded) == LoadResult::Loaded);
    const auto initial = loaded;
    if (scenario == 0) FakeLittleFS::writeBudget = 100;
    if (scenario == 1) FakeNvs::rejectWrite = true;
    if (scenario == 2) FakeNvs::partialWrite = true;
    if (scenario == 3) FakeNvs::failReadAfterWrite = true;
    const auto result = initialize(loaded);
    assert(result == (scenario == 3 ? CommitResult::Indeterminate : CommitResult::NotCommitted));
    assert(loaded.saveVersion == 4 && loaded.boxRoot.storeId == 0 && loaded.sequence == initial.sequence);
    equalState(initial, loaded);
    assert(FakeNvs::data.at("pokemon_g1/save_a") == legacy);
    assert(!FakeLittleFS::files.empty()); // No cleanup, even partial/orphan/uncertain files.
    FakeLittleFS::writeBudget = std::numeric_limits<size_t>::max();
    FakeNvs::rejectWrite = FakeNvs::partialWrite = FakeNvs::failReadAfterWrite = FakeNvs::failRead = false;
    assert(load(reboot) == LoadResult::Loaded);
    assert(reboot.saveVersion == (scenario == 3 ? 5 : 4));
    equalState(initial, reboot);
    if (scenario != 3) assert(initialize(reboot) == CommitResult::Committed);
    const auto saved = encode(reboot);
    assert(load(loaded) == LoadResult::Loaded && encode(loaded) == saved);
  }
}
}
void savePairTests() {
  migration(); initializationFailures(); pairRecovery(); ownershipValidation(); commitsAndMigrationCuts();
  std::puts("PASS B2 pairs: legacy migration/init, guarded format, A/B rollback, global ownership, three-state commits/reboot");
}
