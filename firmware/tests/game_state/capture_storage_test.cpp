#include "capture_test_support.h"
#include "capture_storage.h"
#include <cstdio>
#include <limits>

using namespace PokemonGame;
using CaptureStorage::Result;
std::vector<uint8_t> encode(const GameSave& save);
namespace {
std::string path(const BoxRoot& root) {
  char output[BoxStorage::PATH_SIZE];
  assert(BoxStorage::snapshotPath({root.storeId, root.generation}, output, sizeof(output)));
  return output;
}
void equalPokemon(const PokemonInstance& a, const PokemonInstance& b) {
  uint8_t left[POKEMON_RECORD_SIZE], right[POKEMON_RECORD_SIZE];
  encodePokemonRecord(a, left); encodePokemonRecord(b, right);
  assert(std::memcmp(left, right, sizeof(left)) == 0);
}
void battle(GameSave& save, bool shiny = true) {
  assert(setEncounter(save.state.encounter, 19, 0, 7, Gender::Female, shiny));
  assert(startBattle(save.state));
  save.state.battle.opponent.currentHp = 3;
  save.state.progress.masterBallCount = 2;
  assert(GameSaveStorage::save(save));
}
GameSave fresh(uint8_t partyCount = 3) {
  resetStorageFakes();
  GameSave save;
  assert(GameSaveStorage::load(save) == GameSaveStorage::LoadResult::Missing);
  save.state = createNewGame();
  while (save.state.party.count < partyCount) {
    auto pokemon = save.state.party.members[0];
    pokemon.instanceId = save.state.progress.nextInstanceId++;
    save.state.party.members[save.state.party.count++] = pokemon;
  }
  assert(initializeSave(save));
  battle(save);
  return save;
}
void verifyOwned(const GameSave& before, const GameSave& after, const BattleCaptureReport& report) {
  auto expected = wildPokemon(before.state.battle);
  expected.instanceId = before.state.progress.nextInstanceId;
  expected.exp = 0; expected.friendship = 0;
  equalPokemon(report.caught, expected);
  assert(report.captured && !report.opponentActed);
  assert(after.state.progress.nextInstanceId == before.state.progress.nextInstanceId + 1);
  assert(after.state.progress.masterBallCount + 1 == before.state.progress.masterBallCount);
  assert(after.state.battle.status == BattleStatus::None);
  assert(dexContains(after.state.pokedex.caught, 19));
  assert(dexContains(after.state.pokedex.shinyCaught, 19));
  assert(after.state.progress.deskPetId == before.state.progress.deskPetId);
}
void successAndDuplicates() {
  for (uint8_t count = 1; count <= 3; ++count) {
    auto save = fresh(count); const auto before = save;
    const auto source = *FakeLittleFS::files.at(path(save.boxRoot));
    const auto fileCount = FakeLittleFS::files.size();
    BattleCaptureReport report;
    assert(canUseCaptureBall(save.state, CaptureBall::Master));
    assert(CaptureStorage::attempt(save, CaptureBall::Master, report) == Result::Committed);
    verifyOwned(before, save, report);
    assert(*FakeLittleFS::files.at(path(before.boxRoot)) == source);
    assert(save.state.party.count == (count < 3 ? count + 1 : 3));
    if (count < 3) {
      assert(report.destination == CaptureDestination::Party);
      equalPokemon(save.state.party.members[count], report.caught);
      assert(save.boxRoot.generation == before.boxRoot.generation && save.boxRoot.occupiedCount == 0);
      assert(FakeLittleFS::files.size() == fileCount);
    } else {
      assert(report.destination == CaptureDestination::Box);
      assert(save.boxRoot.generation == before.boxRoot.generation + 1 && save.boxRoot.occupiedCount == 1);
      assert(FakeLittleFS::files.size() == fileCount + 1);
      for (uint8_t i = 0; i < count; ++i) equalPokemon(save.state.party.members[i], before.state.party.members[i]);
      BoxStorage::Snapshot box;
      assert(box.open({save.boxRoot.storeId, save.boxRoot.generation}) == BoxStorage::Result::Ok);
      PokemonInstance actual;
      assert(box.readSlot(0, actual) == BoxStorage::Result::Ok); equalPokemon(actual, report.caught);
    }
    GameSave rebooted;
    assert(GameSaveStorage::load(rebooted) == GameSaveStorage::LoadResult::Loaded);
    assert(encode(save) == encode(rebooted));
    if (count == 3) {
      // A second capture of the same species receives a distinct ID/slot.
      const auto first = report.caught;
      battle(rebooted, false);
      const auto olderPair = rebooted;
      assert(CaptureStorage::attempt(rebooted, CaptureBall::Master, report) == Result::Committed);
      assert(rebooted.boxRoot.occupiedCount == 2 && report.caught.instanceId == first.instanceId + 1);
      assert(report.caught.speciesId == first.speciesId && !report.caught.shiny);
      assert(dexContains(rebooted.state.pokedex.shinyCaught, 19));
      BoxStorage::Snapshot box;
      assert(box.open({rebooted.boxRoot.storeId, rebooted.boxRoot.generation}) == BoxStorage::Result::Ok);
      PokemonInstance actual;
      assert(box.readSlot(0, actual) == BoxStorage::Result::Ok); equalPokemon(actual, first);
      assert(box.readSlot(1, actual) == BoxStorage::Result::Ok); equalPokemon(actual, report.caught);
      box.close();
      GameSave loaded;
      assert(GameSaveStorage::load(loaded) == GameSaveStorage::LoadResult::Loaded);
      assert(encode(loaded) == encode(rebooted));
      // Damage the newer pair only: the other A/B root remains usable.
      FakeLittleFS::files.at(path(rebooted.boxRoot))->back() ^= 1;
      assert(GameSaveStorage::load(loaded) == GameSaveStorage::LoadResult::Loaded);
      assert(encode(loaded) == encode(olderPair));
    }
  }
  std::puts("PASS C capture: Party 1/2 unchanged Box, Box success/fields/dex/IDs, duplicates, reboot and pair rollback");
}
void failedCapture() {
  auto save = fresh();
  GameState expected;
  BattleCaptureReport core;
  for (uint32_t seed = 1; ; ++seed) {
    save.state.battle.rngState = seed; expected = save.state;
    assert(attemptBattleCapture(expected, CaptureBall::Poke, &core, CaptureDestination::Box));
    if (!core.captured) break;
  }
  assert(GameSaveStorage::save(save));
  const auto before = save; const auto files = FakeLittleFS::files.size();
  const auto source = *FakeLittleFS::files.at(path(save.boxRoot));
  BattleCaptureReport report;
  assert(CaptureStorage::attempt(save, CaptureBall::Poke, report) == Result::Committed);
  assert(!report.captured && report.caught.instanceId == 0 && report.opponentActed);
  auto expectedSave = save; expectedSave.state = expected;
  assert(encode(save) == encode(expectedSave));
  assert(save.state.battle.rngState != before.state.battle.rngState);
  assert(save.state.battle.turn == before.state.battle.turn + 1);
  assert(save.state.progress.nextInstanceId == before.state.progress.nextInstanceId);
  assert(save.boxRoot.generation == before.boxRoot.generation && save.boxRoot.snapshotCrc32 == before.boxRoot.snapshotCrc32);
  assert(FakeLittleFS::files.size() == files && *FakeLittleFS::files.at(path(save.boxRoot)) == source);
  std::puts("PASS C failed capture: counterattack/turn/RNG committed, no caught record or Box mutation");
}
void preflight() {
  for (unsigned scenario = 0; scenario < 6; ++scenario) {
    auto save = fresh(); Result expected = Result::StorageError;
    if (scenario == 0) {
      installCaptureBoxFixture(save, BOX_CAPACITY, 2); expected = Result::BoxFull;
    } else if (scenario == 1) {
      save.state.progress.nextInstanceId = UINT32_MAX; assert(GameSaveStorage::save(save));
      expected = Result::IdExhausted;
    } else if (scenario == 2) {
      installCaptureBoxFixture(save, 0, UINT64_MAX); expected = Result::GenerationExhausted;
    } else if (scenario == 3) {
      FakeLittleFS::files.at(path(save.boxRoot))->back() ^= 1;
    } else if (scenario == 4) {
      // Eight occupied candidate paths bound the search without adopting any.
      for (uint64_t generation = 2; generation <= 9; ++generation)
        assert(BoxStorage::createEmpty({save.boxRoot.storeId, generation}) == BoxStorage::Result::Ok);
    } else {
      installCaptureBoxFixture(save, 0, UINT64_MAX - 1);
      assert(BoxStorage::createEmpty({save.boxRoot.storeId, UINT64_MAX}) == BoxStorage::Result::Ok);
      expected = Result::GenerationExhausted;
    }
    const auto before = encode(save); const auto nvs = FakeNvs::data;
    const auto writes = FakeNvs::writes; const auto files = FakeLittleFS::files.size();
    const auto source = *FakeLittleFS::files.at(path(save.boxRoot));
    BattleCaptureReport report;
    assert(CaptureStorage::attempt(save, CaptureBall::Master, report) == expected);
    assert(encode(save) == before && FakeNvs::data == nvs && FakeNvs::writes == writes);
    assert(FakeLittleFS::files.size() == files && *FakeLittleFS::files.at(path(save.boxRoot)) == source);
    assert(!report.captured && report.caught.instanceId == 0);
  }
  // Last representable generation is usable once, without wrapping.
  auto save = fresh(); installCaptureBoxFixture(save, 0, UINT64_MAX - 1);
  BattleCaptureReport report;
  assert(CaptureStorage::attempt(save, CaptureBall::Master, report) == Result::Committed);
  assert(save.boxRoot.generation == UINT64_MAX);
  std::puts("PASS C preflight: full 2048/ID/generation/root/collision guards make no persistent or live changes");
}
void storageFailures() {
  for (unsigned failure = 0; failure < 5; ++failure) {
    auto save = fresh(); const auto before = encode(save); const auto sourceRoot = save.boxRoot;
    const auto source = *FakeLittleFS::files.at(path(sourceRoot));
    const auto writes = FakeNvs::writes;
    if (failure == 0) FakeLittleFS::writeBudget = 100;
    if (failure == 1) FakeLittleFS::corruptFlush = true;
    if (failure == 2) FakeLittleFS::failReopen = true;
    if (failure == 3) FakeLittleFS::truncateClose = true;
    if (failure == 4) FakeNvs::rejectWrite = true;
    BattleCaptureReport report;
    assert(CaptureStorage::attempt(save, CaptureBall::Master, report) ==
      (failure == 4 ? Result::NotCommitted : Result::StorageError));
    assert(encode(save) == before && !report.captured && report.caught.instanceId == 0);
    assert(FakeNvs::writes == writes + (failure == 4 ? 1u : 0u));
    assert(*FakeLittleFS::files.at(path(sourceRoot)) == source);
    assert(FakeLittleFS::files.size() == 1); // Proven exact failed destination removed.
    FakeNvs::rejectWrite = false; FakeLittleFS::failOpen = false;
    FakeLittleFS::writeBudget = std::numeric_limits<size_t>::max();
    assert(GameSaveStorage::validatePair(save) == GameSaveStorage::LoadResult::Loaded);
    assert(CaptureStorage::attempt(save, CaptureBall::Master, report) == Result::Committed);
    assert(save.boxRoot.generation == sourceRoot.generation + 1 && save.boxRoot.occupiedCount == 1);
    assert(FakeLittleFS::files.size() == 2 && report.caught.instanceId + 1 == save.state.progress.nextInstanceId);
    assert(*FakeLittleFS::files.at(path(sourceRoot)) == source);
  }
  std::puts("PASS C write/flush/reopen/validation/NotCommitted failures: old pair intact, orphan collision retry");
}
void uncertain() {
  for (bool partial : {false, true}) {
    auto save = fresh(); const auto before = encode(save);
    FakeNvs::partialWrite = partial; FakeNvs::failReadAfterWrite = true;
    BattleCaptureReport report;
    assert(CaptureStorage::attempt(save, CaptureBall::Master, report) == Result::Indeterminate);
    assert(encode(save) == before && !report.captured && report.caught.instanceId == 0);
    const auto writes = FakeNvs::writes; const auto files = FakeLittleFS::files.size();
    assert(files == 2 && !GameSaveStorage::canWrite());
    FakeNvs::partialWrite = FakeNvs::failReadAfterWrite = FakeNvs::failRead = false;
    assert(CaptureStorage::attempt(save, CaptureBall::Master, report) == Result::StorageError);
    assert(FakeNvs::writes == writes && FakeLittleFS::files.size() == files && encode(save) == before);
    GameSave loaded;
    assert(GameSaveStorage::load(loaded) == GameSaveStorage::LoadResult::Loaded);
    assert(GameSaveStorage::canWrite());
    if (partial) assert(encode(loaded) == before);
    else {
      assert(loaded.boxRoot.occupiedCount == 1 && loaded.state.battle.status == BattleStatus::None);
      assert(loaded.state.progress.nextInstanceId == save.state.progress.nextInstanceId + 1);
      BoxStorage::Snapshot snapshot;
      assert(snapshot.open({loaded.boxRoot.storeId, loaded.boxRoot.generation}) == BoxStorage::Result::Ok);
      PokemonInstance caught;
      assert(snapshot.readSlot(0, caught) == BoxStorage::Result::Ok);
      assert(caught.instanceId == save.state.progress.nextInstanceId && caught.shiny);
    }
    assert(FakeLittleFS::files.size() == files);
  }
  std::puts("PASS C Indeterminate: no live success/delete/retry writes; reboot resolves committed and partial NVS");
}
}
void captureStorageTests() {
  successAndDuplicates(); failedCapture(); preflight(); storageFailures(); uncertain();
}
