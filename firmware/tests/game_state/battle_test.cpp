#include "battle.h"
#include "save_data.h"
#include "save_storage.h"
#include "Preferences.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace PokemonGame;
std::vector<uint8_t> encode(const GameSave& save);
void updateCrc(std::vector<uint8_t>& bytes);
std::vector<uint8_t> legacyV2Record(const GameSave& save);

namespace {
GameState active(SpeciesId id = 19, uint8_t level = 2) {
  auto state = createNewGame();
  assert(setEncounter(state.encounter, id, 0, level, Gender::Female, true));
  assert(startBattle(state));
  return state;
}
std::vector<uint8_t> snapshot(const GameState& state) {
  GameSave save; save.state = state; return encode(save);
}
void roundtrip(const GameState& state) {
  const auto bytes = snapshot(state);
  GameSave restored;
  assert(deserialize(bytes.data(), bytes.size(), restored) == DecodeResult::Ok);
  assert(snapshot(restored.state) == bytes);
}
void typeAndDamage() {
  using T = Type;
  assert(typeEffectiveness(T::Normal,T::Normal) == 4);
  assert(typeEffectiveness(T::Electric,T::Flying) == 8);
  assert(typeEffectiveness(T::Electric,T::Ground,T::Flying) == 0);
  assert(typeEffectiveness(T::Normal,T::Ghost) == 0);
  assert(typeEffectiveness(T::Ice,T::Ground,T::Flying) == 16);
  assert(typeEffectiveness(T::Fire,T::Fire,T::Water) == 1);
  assert(typeEffectiveness(T::Water,T::Grass) == 2);
  assert(typeEffectiveness(T::None,T::Normal) == 0);
  assert(typeEffectiveness(T::Normal,T::None) == 0);
  assert(typeEffectiveness(static_cast<T>(255),T::Normal) == 0);
  assert(typeEffectiveness(T::Normal,T::Normal,static_cast<T>(255)) == 0);
  assert(typeEffectiveness(T::Normal,T::Normal,T::Normal) == 0);
  for (uint8_t a = 1; a <= 18; ++a) for (uint8_t d = 1; d <= 18; ++d) {
    const auto factor = typeEffectiveness(static_cast<T>(a), static_cast<T>(d));
    assert(factor == 0 || factor == 2 || factor == 4 || factor == 8);
  }
  assert(findMove(84)->maxPP == 30 && findMove(45)->category == MoveCategory::Status);
  assert(findMove(33)->category == MoveCategory::Physical && !findMove(999) && !findMove(0));
  assert(findMove(84)->category == MoveCategory::Special);
  auto a = *findSpecies(25), d = *findSpecies(19);
  Stats as{100,100,100,200,100,100}, ds{100,100,100,100,100,100};
  MoveData m = *findMove(33);
  assert(battleDamage(a,50,as,d,ds,m,100) == 19);
  m.category = MoveCategory::Special;
  assert(battleDamage(a,50,as,d,ds,m,100) == 37);
  m.type = T::Electric;
  assert(battleDamage(a,50,as,d,ds,m,100) == 55); // STAB
  d.primaryType = T::Flying;
  assert(battleDamage(a,50,as,d,ds,m,100) == 110);
  d.secondaryType = T::Ground;
  assert(battleDamage(a,50,as,d,ds,m,100) == 0);
  m = *findMove(45);
  assert(battleDamage(a,50,as,d,ds,m,100) == 0);
  m = *findMove(33); ds.defense = 65535; as.attack = 1;
  assert(battleDamage(a,1,as,d,ds,m,85) >= 1);
  ds.defense = 0;
  assert(battleDamage(a,100,as,d,ds,m,100) == 0);
  as.attack = 65535; ds.defense = 1; m.power = 65535;
  assert(battleDamage(a,100,as,d,ds,m,100) == 65535);
  uint32_t r = 1;
  assert(!moveHits(0,r)); assert(moveHits(100,r));
  r = 1; assert(!moveHits(50,r)); // xorshift32(1)=270369 =>69
  r = 2; assert(moveHits(50,r));  // xorshift32(2)=540738 =>38
  r = 0; assert(battleRandom(r) && r);
}
void turns() {
  auto legacyReady = createNewGame();
  assert(startExploration(legacyReady.exploration,1,1800000000,15));
  assert(setEncounter(legacyReady.encounter,16,0,3,Gender::Female,false));
  assert(startBattle(legacyReady) && legacyReady.exploration.status == ExplorationStatus::Idle);
  auto state = active();
  assert(state.battle.playerId == partner(state)->instanceId);
  assert(state.battle.wild.speciesId == 19 && state.battle.wild.level == 2);
  assert(state.battle.wild.gender == Gender::Female && state.battle.wild.shiny);
  assert(state.encounter.status == EncounterStatus::None && state.exploration.status == ExplorationStatus::Idle);
  assert(state.battle.player.currentHp == 18 && state.battle.opponent.currentHp == 13);
  assert(state.battle.player.pp[0] == 30 && state.battle.player.pp[1] == 40);
  assert(state.battle.opponent.pp[0] == 35 && state.battle.turn == 0 && state.battle.rngState);
  assert(nextBattleMove(*partner(state),state.battle,0,1) == 1);
  assert(nextBattleMove(*partner(state),state.battle,0,-1) == 1);
  auto repeat = state;
  BattleTurnReport report;
  assert(resolveBattleTurn(state,0,&report) && resolveBattleTurn(repeat,0));
  assert(report.count == 2 && report.actions[0].actor == BattleActor::Player);
  assert(report.actions[1].actor == BattleActor::Wild && report.actions[0].moveId == 84);
  assert(report.actions[1].moveId == 33 && report.actions[0].hit && report.actions[1].hit);
  assert(report.actions[0].damage == 13 - state.battle.opponent.currentHp);
  assert(report.actions[1].damage == 18 - state.battle.player.currentHp);
  static_assert(SAVE_VERSION == 4 && BATTLE_RECORD_SIZE == 40 && SAVE_MAX_SIZE == 554, "G4 wire format");
  assert(snapshot(state) == snapshot(repeat));
  assert(state.battle.player.pp[0] == 29 && state.battle.opponent.pp[0] == 34);
  assert(state.battle.player.currentHp < 18 && state.battle.opponent.currentHp < 13);
  assert(state.battle.turn == 1);
  roundtrip(state);
  // 실제 turn resolver에서 명중/빗나감 및 PP 소모를 함께 확인한다.
  auto miss = createNewGame(); miss.party.members[0].moves[0] = 21;
  assert(setEncounter(miss.encounter,19,0,2,Gender::Male,false));
  assert(startBattle(miss)); miss.battle.rngState = 1;
  const auto hpBefore = miss.battle.opponent.currentHp;
  assert(resolveBattleTurn(miss,0,&report));
  assert(report.count == 2 && !report.actions[0].hit && report.actions[0].damage == 0);
  assert(!report.actions[0].fainted && report.actions[0].moveId == 21);
  assert(miss.battle.opponent.currentHp == hpBefore && miss.battle.player.pp[0] == 19);
  auto hit = createNewGame(); hit.party.members[0].moves[0] = 21;
  assert(setEncounter(hit.encounter,19,0,2,Gender::Male,false));
  assert(startBattle(hit)); hit.battle.rngState = 2;
  assert(resolveBattleTurn(hit,0) && hit.battle.opponent.currentHp < hpBefore);
  auto maxTurn = active(); maxTurn.battle.turn = UINT32_MAX;
  assert(resolveBattleTurn(maxTurn,0) && maxTurn.battle.turn == UINT32_MAX);
  auto status = active();
  assert(resolveBattleTurn(status,1,&report));
  assert(report.actions[0].hit && report.actions[0].moveId == 45 && report.actions[0].damage == 0);
  assert(status.battle.player.pp[1] == 39 && status.battle.opponent.currentHp == 13);
  status.battle.player.pp[0] = 0;
  auto before = snapshot(status);
  const auto oldCount = report.count;
  assert(!resolveBattleTurn(status,0,&report) && snapshot(status) == before && report.count == oldCount);
  for (auto& pp : status.battle.player.pp) pp = 0;
  for (auto& pp : status.battle.opponent.pp) pp = 0;
  assert(canSelectMove(*partner(status),status.battle,STRUGGLE_SLOT));
  assert(nextBattleMove(*partner(status),status.battle,0,1) == STRUGGLE_SLOT);
  assert(resolveBattleTurn(status,STRUGGLE_SLOT));
  assert(status.battle.opponent.currentHp < 13);
  // 빠른 플레이어가 기절시키면 야생 PP는 소모되지 않는다.
  auto won = active(); won.battle.opponent.currentHp = 1;
  assert(resolveBattleTurn(won,0,&report));
  assert(report.count == 1 && report.actions[0].damage == 1 && report.actions[0].fainted);
  assert(won.battle.status == BattleStatus::Won && won.battle.opponent.pp[0] == 35);
  assert(won.battle.player.currentHp == 18);
  roundtrip(won);
  auto lost = active(25,100); lost.battle.player.currentHp = 1;
  assert(resolveBattleTurn(lost,0,&report));
  assert(report.count == 1 && report.actions[0].actor == BattleActor::Wild && report.actions[0].fainted);
  assert(lost.battle.status == BattleStatus::Lost && lost.battle.player.pp[0] == 30);
  roundtrip(lost);
  // 같은 속도: 같은 seed는 같은 순서. seed를 달리하면 양쪽 순서 모두 나온다.
  bool playerFirst = false, wildFirst = false;
  for (uint32_t seed = 1; seed < 30; ++seed) {
    auto tie = active(25,5); tie.battle.player.currentHp = 1; tie.battle.opponent.currentHp = 1;
    tie.battle.rngState = seed; auto copy = tie;
    assert(resolveBattleTurn(tie,0) && resolveBattleTurn(copy,0));
    assert(snapshot(tie) == snapshot(copy));
    playerFirst |= tie.battle.status == BattleStatus::Won;
    wildFirst |= tie.battle.status == BattleStatus::Lost;
  }
  assert(playerFirst && wildFirst);
  assert(acknowledgeBattle(lost) && lost.battle.status == BattleStatus::None);
  assert(partner(lost)->currentHp == 18 && partner(lost)->exp == 0);
  assert(!acknowledgeBattle(lost));
  auto zeroHp = createNewGame(); zeroHp.party.members[0].currentHp = 0;
  assert(setEncounter(zeroHp.encounter,16,0,3,Gender::Male,false));
  assert(startBattle(zeroHp) && zeroHp.battle.status == BattleStatus::Lost);
  assert(acknowledgeBattle(zeroHp) && partner(zeroHp)->currentHp == 18);
  // fixture 전 종에서 PP를 소진해도 유한 턴 안에 결과에 도달한다.
  for (SpeciesId id : {SpeciesId(16),SpeciesId(19),SpeciesId(25)}) {
    auto play = active(id,4);
    for (auto& pp : play.battle.player.pp) pp = 0;
    for (auto& pp : play.battle.opponent.pp) pp = 0;
    unsigned n = 0;
    while (play.battle.status == BattleStatus::Active && n++ < 100)
      assert(resolveBattleTurn(play,STRUGGLE_SLOT));
    assert(play.battle.status != BattleStatus::Active);
  }
}
void validationAndMigration() {
  auto state = active();
  const auto bytes = snapshot(state);
  // v4 Battle record: status0 id1 wild5 moves12 playerHP20/PP22 wildHP26/PP28 turn32 rng36.
  for (const size_t offset : {size_t(0),size_t(1),size_t(5),size_t(6),size_t(8),size_t(9),size_t(10),
       size_t(11),size_t(12),size_t(20),size_t(22),size_t(24),size_t(26),size_t(28),size_t(36)}) {
    auto bad = bytes; const size_t pos = bytes.size() - BATTLE_RECORD_SIZE;
    bad[pos + offset] = 255;
    if (offset == 36) for (size_t i = 0; i < 4; ++i) bad[pos+36+i] = 0;
    updateCrc(bad);
    GameSave output; output.state = createNewGame(); const auto old = encode(output);
    assert(deserialize(bad.data(),bad.size(),output) == DecodeResult::Invalid);
    assert(encode(output) == old);
  }
  auto invalid = state; invalid.battle.status = BattleStatus::Won; assert(!isValidState(invalid));
  invalid = state; invalid.battle.status = BattleStatus::Lost; assert(!isValidState(invalid));
  invalid = state; invalid.battle.status = BattleStatus::None; assert(!isValidState(invalid));
  invalid = state; invalid.encounter = state.battle.wild; assert(!isValidState(invalid));
  invalid = state; assert(startExploration(invalid.exploration,1,1800000000,15)); assert(!isValidState(invalid));
  // v3에 있던 모든 필드는 유지하고 Battle만 None으로 추가한다.
  GameSave old; old.state = createNewGame(); old.sequence = 82;
  old.state.party.members[0].exp = 123; old.state.party.members[0].friendship = 99;
  assert(startExploration(old.state.exploration,1,1800000000,15));
  assert(updateExploration(old.state.exploration,1800000015));
  assert(setEncounter(old.state.encounter,16,0,3,Gender::Female,true));
  auto v3 = legacyV2Record(old);
  // Frozen G3 필드 순서. v4 encoder를 사용하지 않는다.
  const auto& e = old.state.encounter;
  v3.push_back(static_cast<uint8_t>(e.status));
  v3.push_back(static_cast<uint8_t>(e.speciesId));
  v3.push_back(static_cast<uint8_t>(e.speciesId >> 8));
  v3.push_back(e.formId); v3.push_back(e.level);
  v3.push_back(static_cast<uint8_t>(e.gender)); v3.push_back(e.shiny ? 1 : 0);
  v3[4]=3;
  const uint32_t length = static_cast<uint32_t>(v3.size()-SAVE_HEADER_SIZE);
  for (unsigned i=0;i<4;++i) v3[10+i]=static_cast<uint8_t>(length>>(8*i));
  updateCrc(v3);
  FakeNvs::reset(); FakeNvs::data["pokemon_g1/save_a"] = v3;
  GameSave loaded;
  assert(GameSaveStorage::load(loaded) == GameSaveStorage::LoadResult::Loaded && loaded.saveVersion == 3);
  assert(loaded.state.battle.status == BattleStatus::None);
  loaded.saveVersion = SAVE_VERSION;
  assert(encode(loaded) == encode(old));
  loaded.saveVersion = 3;
  FakeNvs::partialWrite = true; assert(!GameSaveStorage::save(loaded));
  assert(loaded.saveVersion == 3 && FakeNvs::data.at("pokemon_g1/save_a") == v3);
  FakeNvs::partialWrite = false; assert(GameSaveStorage::save(loaded));
  assert(loaded.saveVersion == 4 && loaded.sequence == 83);
  GameSave reboot;
  assert(GameSaveStorage::load(reboot) == GameSaveStorage::LoadResult::Loaded);
  assert(encode(loaded) == encode(reboot));
}
}
void battleTests() {
  typeAndDamage(); turns(); validationAndMigration();
  std::puts("PASS battle: types, moves, damage/STAB, accuracy, PP, speed/ties, faint, results, v4 codec, v3 migration");
}
