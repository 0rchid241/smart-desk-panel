#include "storage_test_support.h"
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
  GameSave save;
  save.state = state;
  return encode(save);
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
  static_assert(SAVE_VERSION == 5 && BATTLE_RECORD_SIZE == 40 && SAVE_V4_MAX_SIZE == 554 && SAVE_MAX_SIZE == 586, "G4 wire format");
  assert(snapshot(state) == snapshot(repeat));
  assert(state.battle.player.pp[0] == 29 && state.battle.opponent.pp[0] == 34);
  assert(state.battle.player.currentHp < 18 && state.battle.opponent.currentHp < 13);
  assert(state.battle.turn == 1);
  roundtrip(state);

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

void running() {
  // seed 3의 첫 xorshift roll은 7. fixture 속도 범위에서 항상 성공한다.
  auto escaped = active();
  escaped.battle.rngState = 3;
  BattleRunReport success;
  assert(attemptBattleRun(escaped, &success));
  assert(success.escaped && !success.opponentActed);
  assert(escaped.battle.status == BattleStatus::None);
  assert(partner(escaped)->currentHp == 18);
  assert(isValidState(escaped));
  roundtrip(escaped);

  // seed 4의 첫 roll은 76. 테스트 fixture의 도주 확률(64~74%)보다 높아서 실패한다.
  auto failed = active();
  failed.battle.rngState = 4;
  auto repeat = failed;
  BattleRunReport report;
  BattleRunReport repeatReport;
  assert(attemptBattleRun(failed, &report));
  assert(attemptBattleRun(repeat, &repeatReport));
  assert(!report.escaped && report.opponentActed);
  assert(!repeatReport.escaped && repeatReport.opponentActed);
  assert(report.opponentAction.actor == BattleActor::Wild);
  assert(report.opponentAction.moveId == 33);
  assert(report.opponentAction.hit);
  assert(report.opponentAction.damage == 18 - failed.battle.player.currentHp);
  assert(failed.battle.opponent.currentHp == 13);
  assert(failed.battle.opponent.pp[0] == 34);
  assert(failed.battle.turn == 1);
  assert(failed.battle.status == BattleStatus::Active);
  assert(snapshot(failed) == snapshot(repeat));
  assert(report.opponentAction.damage == repeatReport.opponentAction.damage);
  assert(report.opponentAction.hit == repeatReport.opponentAction.hit);
  roundtrip(failed);

  // 실패 반격으로 쓰러지면 Lost가 된다.
  auto fainted = active();
  fainted.battle.player.currentHp = 1;
  fainted.battle.rngState = 4;
  BattleRunReport faintReport;
  assert(attemptBattleRun(fainted, &faintReport));
  assert(!faintReport.escaped && faintReport.opponentActed);
  assert(faintReport.opponentAction.fainted);
  assert(faintReport.opponentAction.damage == 1);
  assert(fainted.battle.status == BattleStatus::Lost);
  assert(fainted.battle.turn == 1);
  roundtrip(fainted);

  // PP가 모두 없으면 실패 반격도 발버둥으로 진행한다.
  auto struggle = active();
  for (auto& pp : struggle.battle.opponent.pp) pp = 0;
  struggle.battle.rngState = 4;
  BattleRunReport struggleReport;
  assert(attemptBattleRun(struggle, &struggleReport));
  assert(!struggleReport.escaped && struggleReport.opponentActed);
  assert(struggleReport.opponentAction.moveId == 0);

  // 유효하지 않은 상태에서는 state/report를 건드리지 않는다.
  auto invalid = escaped; // 이미 Battle None.
  BattleRunReport untouched;
  untouched.escaped = true;
  untouched.opponentActed = true;
  const auto invalidBefore = snapshot(invalid);
  assert(!attemptBattleRun(invalid, &untouched));
  assert(snapshot(invalid) == invalidBefore);
  assert(untouched.escaped && untouched.opponentActed);

  static_assert(SAVE_VERSION == 5 && BATTLE_RECORD_SIZE == 40, "G5-A2 keeps v4 save layout");
}

void capturing() {
  auto state = active();

  // 새 게임은 ballTier 0이라 몬스터볼만 기본 사용 가능하다.
  assert(canUseCaptureBall(state, CaptureBall::Poke));
  assert(!canUseCaptureBall(state, CaptureBall::Great));
  assert(!canUseCaptureBall(state, CaptureBall::Ultra));
  assert(!canUseCaptureBall(state, CaptureBall::Master));
  assert(captureChance(state, CaptureBall::Poke) == 60);

  // ballTier는 하위 일반 볼을 모두 해금한다. 마스터볼은 별도 개수다.
  state.progress.ballTier = 2;
  state.progress.masterBallCount = 1;
  assert(canUseCaptureBall(state, CaptureBall::Great));
  assert(canUseCaptureBall(state, CaptureBall::Ultra));
  assert(canUseCaptureBall(state, CaptureBall::Master));
  assert(captureChance(state, CaptureBall::Great) == 78);
  assert(captureChance(state, CaptureBall::Ultra) == 95);
  assert(captureChance(state, CaptureBall::Master) == 100);

  // HP가 낮을수록 포획률이 올라가며 일반 볼은 95%에서 제한한다.
  auto weakened = active();
  weakened.battle.opponent.currentHp = 1;
  assert(captureChance(weakened, CaptureBall::Poke) == 95);

  // seed 3의 첫 roll=7: 풀피 꼬렛도 몬스터볼(60%) 포획 성공.
  auto caught = active();
  caught.battle.rngState = 3;
  BattleCaptureReport caughtReport;
  assert(attemptBattleCapture(caught, CaptureBall::Poke, &caughtReport));
  assert(caughtReport.captured && !caughtReport.opponentActed);
  assert(caughtReport.ball == CaptureBall::Poke);
  assert(caughtReport.chance == 60);
  assert(caughtReport.wild.status == EncounterStatus::Ready);
  assert(caughtReport.wild.speciesId == 19);
  assert(caught.battle.status == BattleStatus::None);
  assert(isValidState(caught));
  roundtrip(caught);

  // seed 4의 첫 roll=76: 풀피 꼬렛 포획 실패 후 야생이 한 번 행동한다.
  auto failed = active();
  failed.battle.rngState = 4;
  auto repeat = failed;
  BattleCaptureReport failReport;
  BattleCaptureReport repeatReport;
  assert(attemptBattleCapture(failed, CaptureBall::Poke, &failReport));
  assert(attemptBattleCapture(repeat, CaptureBall::Poke, &repeatReport));
  assert(!failReport.captured && failReport.opponentActed);
  assert(failReport.chance == 60);
  assert(failReport.opponentAction.actor == BattleActor::Wild);
  assert(failReport.opponentAction.moveId == 33);
  assert(failReport.opponentAction.hit);
  assert(failed.battle.status == BattleStatus::Active);
  assert(failed.battle.turn == 1);
  assert(failed.battle.opponent.pp[0] == 34);
  assert(snapshot(failed) == snapshot(repeat));
  assert(failReport.opponentAction.damage == repeatReport.opponentAction.damage);
  roundtrip(failed);

  // 실패 반격으로 쓰러지면 기존 패배 결과로 연결한다.
  auto fainted = active();
  fainted.battle.player.currentHp = 1;
  fainted.battle.rngState = 4;
  BattleCaptureReport faintReport;
  assert(attemptBattleCapture(fainted, CaptureBall::Poke, &faintReport));
  assert(!faintReport.captured && faintReport.opponentActed);
  assert(faintReport.opponentAction.fainted);
  assert(fainted.battle.status == BattleStatus::Lost);
  assert(fainted.battle.turn == 1);
  roundtrip(fainted);

  // 마스터볼은 RNG와 무관하게 성공하고 저장 후보에서만 1개 소비한다.
  auto master = active();
  master.progress.masterBallCount = 2;
  master.battle.rngState = 13;
  BattleCaptureReport masterReport;
  assert(attemptBattleCapture(master, CaptureBall::Master, &masterReport));
  assert(masterReport.captured && masterReport.chance == 100);
  assert(master.progress.masterBallCount == 1);
  assert(master.battle.status == BattleStatus::None);
  roundtrip(master);

  // 잠긴 볼/비전투 상태에서는 state와 report를 건드리지 않는다.
  auto locked = active();
  BattleCaptureReport untouched;
  untouched.captured = true;
  untouched.opponentActed = true;
  const auto lockedBefore = snapshot(locked);
  assert(!attemptBattleCapture(locked, CaptureBall::Great, &untouched));
  assert(snapshot(locked) == lockedBefore);
  assert(untouched.captured && untouched.opponentActed);

  auto noBattle = caught;
  const auto noBattleBefore = snapshot(noBattle);
  assert(!attemptBattleCapture(noBattle, CaptureBall::Poke, &untouched));
  assert(snapshot(noBattle) == noBattleBefore);

  static_assert(
    SAVE_VERSION == 5 &&
    BATTLE_RECORD_SIZE == 40,
    "G5-B keeps v4 save layout"
  );
}

void captureOwnership() {
  static_assert(SAVE_VERSION == 5 && POKEMON_RECORD_SIZE == 24 &&
    BATTLE_RECORD_SIZE == 40 && PARTY_CAPACITY == 3, "C1 keeps v4 layout");
  for (bool shiny : {false, true}) {
    auto state = active(19, 7);
    state.battle.wild.shiny = shiny;
    state.battle.opponent.currentHp = 3;
    state.progress.masterBallCount = 2;
    const auto before = state;
    BattleCaptureReport report;
    assert(attemptBattleCapture(state,CaptureBall::Master,&report) && report.captured);
    assert(state.party.count == 2 && state.progress.nextInstanceId == before.progress.nextInstanceId + 1);
    const auto& caught = state.party.members[1];
    assert(caught.instanceId == before.progress.nextInstanceId);
    assert(caught.speciesId == before.battle.wild.speciesId && caught.formId == before.battle.wild.formId);
    assert(caught.level == before.battle.wild.level && caught.gender == before.battle.wild.gender);
    assert(caught.shiny == shiny && caught.currentHp == 3 && caught.exp == 0 && caught.friendship == 0);
    for (int i=0;i<4;++i) assert(caught.moves[i] == before.battle.wildMoves[i]);
    assert(dexContains(state.pokedex.seen,19) && dexContains(state.pokedex.caught,19));
    assert(dexContains(state.pokedex.shinyCaught,19) == shiny);
    assert(state.progress.deskPetId == before.progress.deskPetId && partner(state)->instanceId == 1);
    assert(state.progress.masterBallCount == 1 && state.battle.status == BattleStatus::None);
    // 포획된 두 번째 슬롯을 제외한 기존 파티 전체를 별도로 비교한다.
    auto original = before; original.battle = BattleState{};
    auto prefix = state; prefix.party.count = 1; prefix.progress = original.progress; prefix.pokedex = original.pokedex;
    assert(snapshot(prefix) == snapshot(original));
    assert(isValidState(state)); roundtrip(state);

    // 같은 종을 다시 포획하되 일반 개체는 기존 shiny 등록을 지우지 않는다.
    const auto firstCaughtId = caught.instanceId;
    assert(setEncounter(state.encounter,19,0,2,Gender::Male,false) && startBattle(state));
    assert(attemptBattleCapture(state,CaptureBall::Master,&report));
    assert(state.party.count == 3 && state.party.members[1].instanceId == firstCaughtId);
    assert(state.party.members[2].instanceId == firstCaughtId + 1);
    assert(state.party.members[2].speciesId == 19 && !state.party.members[2].shiny);
    assert(dexContains(state.pokedex.shinyCaught,19) == shiny);
    assert(isValidState(state)); roundtrip(state);

    // 볼은 사용 가능해도 기본 Party 목적지는 RNG/수량/보고서 변경 없이 거부한다.
    assert(setEncounter(state.encounter,16,0,3,Gender::Male,false) && startBattle(state));
    state.progress.ballTier = 2; state.progress.masterBallCount = 2;
    const auto full = snapshot(state);
    for (auto ball : {CaptureBall::Poke,CaptureBall::Great,CaptureBall::Ultra,CaptureBall::Master}) {
      BattleCaptureReport untouched; untouched.chance = 123;
      assert(canUseCaptureBall(state,ball));
      assert(!attemptBattleCapture(state,ball,&untouched));
      assert(snapshot(state) == full && untouched.chance == 123);
    }
  }
  auto exhaustedId = active(); exhaustedId.progress.nextInstanceId = UINT32_MAX;
  exhaustedId.progress.masterBallCount = 1;
  assert(isValidState(exhaustedId));
  auto old = snapshot(exhaustedId);
  assert(!attemptBattleCapture(exhaustedId,CaptureBall::Master) && snapshot(exhaustedId) == old);
  assert(!attemptBattleCapture(exhaustedId,CaptureBall::Poke) && snapshot(exhaustedId) == old);
  exhaustedId.progress.nextInstanceId = UINT32_MAX - 1;
  assert(attemptBattleCapture(exhaustedId,CaptureBall::Master));
  assert(exhaustedId.party.members[1].instanceId == UINT32_MAX - 1);
  assert(exhaustedId.progress.nextInstanceId == UINT32_MAX && isValidState(exhaustedId));
  roundtrip(exhaustedId);
  auto invalidId = active(); invalidId.progress.nextInstanceId = 0;
  assert(!attemptBattleCapture(invalidId,CaptureBall::Poke) && invalidId.progress.nextInstanceId == 0);
  std::puts("PASS C1 ownership: wild fields/HP/moves, IDs, duplicate species, dex, full party, overflow, v4 roundtrip");
}

void validationAndMigration() {
  auto state = active();
  const auto bytes = snapshot(state);
  for (const size_t offset : {size_t(0),size_t(1),size_t(5),size_t(6),size_t(8),size_t(9),size_t(10),
       size_t(11),size_t(12),size_t(20),size_t(22),size_t(24),size_t(26),size_t(28),size_t(36)}) {
    auto bad = bytes; const size_t pos = bytes.size() - BOX_ROOT_RECORD_SIZE - BATTLE_RECORD_SIZE;
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

  GameSave old; old.state = createNewGame(); old.sequence = 82;
  old.state.party.members[0].exp = 123; old.state.party.members[0].friendship = 99;
  assert(startExploration(old.state.exploration,1,1800000000,15));
  assert(updateExploration(old.state.exploration,1800000015));
  assert(setEncounter(old.state.encounter,16,0,3,Gender::Female,true));
  auto v3 = legacyV2Record(old);
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
  resetStorageFakes(); FakeNvs::data["pokemon_g1/save_a"] = v3;
  GameSave loaded;
  assert(GameSaveStorage::load(loaded) == GameSaveStorage::LoadResult::Loaded && loaded.saveVersion == 3);
  assert(loaded.state.battle.status == BattleStatus::None);
  loaded.saveVersion = SAVE_VERSION;
  assert(encode(loaded) == encode(old));
  loaded.saveVersion = 3;
  FakeNvs::partialWrite = true; assert(!initializeSave(loaded));
  assert(loaded.saveVersion == 3 && FakeNvs::data.at("pokemon_g1/save_a") == v3);
  FakeNvs::partialWrite = false; assert(initializeSave(loaded));
  assert(loaded.saveVersion == 5 && loaded.sequence == 83);
  GameSave reboot;
  assert(GameSaveStorage::load(reboot) == GameSaveStorage::LoadResult::Loaded);
  assert(encode(loaded) == encode(reboot));
}
}

void battleTests() {
  captureOwnership();
  typeAndDamage();
  turns();
  running();
  capturing();
  validationAndMigration();
  std::puts("PASS battle: types, moves, turns, run, capture chance/success/failure, counterattack, v4 codec, v3 migration");
}
