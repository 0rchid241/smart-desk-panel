#include "battle.h"
#include "game_state.h"

namespace PokemonGame {
PokemonInstance wildPokemon(const BattleState& b) {
  PokemonInstance p;
  p.instanceId = 1; // 계산용 임시 값이며 저장/소유 ID가 아니다.
  p.speciesId = b.wild.speciesId; p.formId = b.wild.formId;
  p.level = b.wild.level; p.gender = b.wild.gender; p.shiny = b.wild.shiny;
  p.currentHp = b.opponent.currentHp;
  for (int i = 0; i < 4; ++i) p.moves[i] = b.wildMoves[i];
  return p;
}
namespace {
bool validPP(const MoveId (&moves)[4], const BattleCombatant& p) {
  for (int i = 0; i < 4; ++i) {
    const auto* m = findMove(moves[i]);
    if (moves[i] && !m) return false;
    if (p.pp[i] > (m ? m->maxPP : 0)) return false;
  }
  return true;
}
bool emptyCombatant(const BattleCombatant& c) {
  if (c.currentHp) return false;
  for (auto pp : c.pp) if (pp) return false;
  return true;
}
void initCombatant(BattleCombatant& c, const PokemonInstance& p) {
  c.currentHp = p.currentHp;
  for (int i = 0; i < 4; ++i) {
    const auto* m = findMove(p.moves[i]);
    c.pp[i] = m ? m->maxPP : 0;
  }
}
}
bool isValidBattle(const BattleState& b, const PokemonInstance* p) {
  if (b.status == BattleStatus::None) {
    if (b.playerId || b.turn || b.rngState || b.wild.status != EncounterStatus::None ||
        !isValidEncounter(b.wild) || !emptyCombatant(b.player) || !emptyCombatant(b.opponent)) return false;
    for (auto id : b.wildMoves) if (id) return false;
    return true;
  }
  if (!p || !isValidPokemon(*p) || b.playerId != p->instanceId || !b.rngState ||
      !isValidEncounter(b.wild) || b.wild.status != EncounterStatus::Ready ||
      !validPP(p->moves, b.player) || !validPP(b.wildMoves, b.opponent) ||
      !isValidPokemon(wildPokemon(b)) || b.player.currentHp > p->currentHp) return false;
  MoveId expected[4]; fixtureMoves(b.wild.speciesId, expected);
  for (int i = 0; i < 4; ++i) if (expected[i] != b.wildMoves[i]) return false;
  switch (b.status) {
    case BattleStatus::Active:
      return b.player.currentHp && b.opponent.currentHp;
    case BattleStatus::Won: return b.player.currentHp && !b.opponent.currentHp && b.turn > 0;
    case BattleStatus::Lost:
      return !b.player.currentHp && b.opponent.currentHp && (b.turn > 0 || p->currentHp == 0);
    default: return false;
  }
}
uint32_t battleRandom(uint32_t& state) {
  if (!state) state = 0x6d2b79f5u;
  state ^= state << 13; state ^= state >> 17; state ^= state << 5;
  return state;
}
bool moveHits(uint8_t accuracy, uint32_t& rng) {
  return accuracy <= 100 && battleRandom(rng) % 100 < accuracy;
}
uint16_t battleDamage(const PokemonSpecies& a, uint8_t level, const Stats& as,
                      const PokemonSpecies& d, const Stats& ds,
                      const MoveData& m, uint8_t variation) {
  if (!level || level > 100 || !m.power || m.category == MoveCategory::Status ||
      variation < 85 || variation > 100) return 0;
  const uint16_t attack = m.category == MoveCategory::Physical ? as.attack : as.spAttack;
  const uint16_t defense = m.category == MoveCategory::Physical ? ds.defense : ds.spDefense;
  if (!defense) return 0;
  const unsigned effectiveness = m.type == Type::None ? 4 :
    typeEffectiveness(m.type, d.primaryType, d.secondaryType);
  if (!effectiveness) return 0;
  uint64_t damage = (static_cast<uint64_t>(2u * level / 5u + 2u) * m.power * attack / defense / 50u) + 2u;
  if (m.type != Type::None && (m.type == a.primaryType || m.type == a.secondaryType)) damage = damage * 3 / 2;
  damage = damage * effectiveness * variation / 400;
  if (!damage) damage = 1;
  return static_cast<uint16_t>(damage > UINT16_MAX ? UINT16_MAX : damage);
}
bool canSelectMove(const PokemonInstance& p, const BattleState& b, uint8_t slot) {
  if (slot < 4) return findMove(p.moves[slot]) && b.player.pp[slot];
  if (slot != STRUGGLE_SLOT) return false;
  for (uint8_t i = 0; i < 4; ++i) if (canSelectMove(p, b, i)) return false;
  return true;
}
uint8_t nextBattleMove(const PokemonInstance& p, const BattleState& b, uint8_t current, int direction) {
  for (int step = 1; step <= 5; ++step) {
    const auto slot = static_cast<uint8_t>((static_cast<int>(current % 5) + (direction < 0 ? -step : step) + 5) % 5);
    if (canSelectMove(p, b, slot)) return slot;
  }
  return STRUGGLE_SLOT;
}
bool startBattle(GameState& state) {
  if (!isValidState(state) || state.battle.status != BattleStatus::None ||
      state.encounter.status != EncounterStatus::Ready) return false;
  auto next = state;
  const auto& p = *partner(next);
  auto& b = next.battle;
  b.status = p.currentHp ? BattleStatus::Active : BattleStatus::Lost;
  b.playerId = p.instanceId; b.wild = next.encounter;
  fixtureMoves(b.wild.speciesId, b.wildMoves);
  auto wild = wildPokemon(b); wild.currentHp = calculateStats(wild).hp;
  initCombatant(b.player, p); initCombatant(b.opponent, wild);
  b.rngState = makeTestEncounterRoll(next.exploration) ^ p.instanceId ^
    (static_cast<uint32_t>(b.wild.speciesId) << 16) ^ b.wild.level ^
    (static_cast<uint32_t>(b.wild.gender) << 8) ^ (b.wild.shiny ? 0x80000000u : 0);
  if (!b.rngState) b.rngState = 0x6d2b79f5u;
  // 이전 버전에서 허용했던 탐험/Ready 조합도 이미 확정된 조우를 우선한다.
  next.encounter = EncounterState{}; next.exploration = ExplorationSession{};
  if (!isValidState(next)) return false;
  state = next; return true;
}
bool resolveBattleTurn(GameState& state, uint8_t slot, BattleTurnReport* report) {
  if (!isValidState(state) || state.battle.status != BattleStatus::Active ||
      !canSelectMove(*partner(state), state.battle, slot)) return false;
  auto next = state;
  auto& b = next.battle;
  BattleTurnReport turnReport;
  const auto& p = *partner(next);
  const auto wild = wildPokemon(b);
  const auto ps = calculateStats(p), ws = calculateStats(wild);
  uint8_t options[4] = {}, count = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    const auto* m = findMove(wild.moves[i]);
    if (m && m->category != MoveCategory::Status && b.opponent.pp[i]) options[count++] = i;
  }
  // 공격 PP 소진 후 남은 변화 기술도 사용하고, 모두 소진하면 발버둥.
  if (!count) for (uint8_t i = 0; i < 4; ++i)
    if (findMove(wild.moves[i]) && b.opponent.pp[i]) options[count++] = i;
  const uint8_t wildSlot = count ? options[battleRandom(b.rngState) % count] : STRUGGLE_SLOT;
  const bool playerFirst = ps.speed == ws.speed ? (battleRandom(b.rngState) & 1u) != 0 : ps.speed > ws.speed;
  auto act = [&](bool player) {
    auto& own = player ? b.player : b.opponent;
    auto& other = player ? b.opponent : b.player;
    const auto& attacker = player ? p : wild;
    const auto& defender = player ? wild : p;
    const uint8_t selected = player ? slot : wildSlot;
    const auto& m = selected == STRUGGLE_SLOT ? struggleMove() : *findMove(attacker.moves[selected]);
    auto& action = turnReport.actions[turnReport.count++];
    action.actor = player ? BattleActor::Player : BattleActor::Wild;
    action.moveId = m.id;
    const auto* defenderSpecies = findSpecies(defender.speciesId, defender.formId);
    action.effectiveness = m.type == Type::None ? 4 :
      typeEffectiveness(m.type, defenderSpecies->primaryType, defenderSpecies->secondaryType);
    if (selected < 4) --own.pp[selected];
    action.hit = moveHits(m.accuracy, b.rngState);
    if (!action.hit) return;
    if (m.category == MoveCategory::Status) return;
    const auto variation = static_cast<uint8_t>(85 + battleRandom(b.rngState) % 16);
    const uint16_t damage = battleDamage(*findSpecies(attacker.speciesId, attacker.formId), attacker.level,
      player ? ps : ws, *findSpecies(defender.speciesId, defender.formId), player ? ws : ps, m, variation);
    action.damage = damage >= other.currentHp ? other.currentHp : damage;
    other.currentHp = static_cast<uint16_t>(other.currentHp - action.damage);
    action.fainted = other.currentHp == 0;
  };
  act(playerFirst);
  if (b.player.currentHp && b.opponent.currentHp) act(!playerFirst);
  // 극단적인 저장 값에서도 wrap/진행 불능을 피한다.
  if (b.turn != UINT32_MAX) ++b.turn;
  if (!b.opponent.currentHp) b.status = BattleStatus::Won;
  else if (!b.player.currentHp) b.status = BattleStatus::Lost;
  if (!isValidState(next)) return false;
  state = next;
  if (report) *report = turnReport;
  return true;
}
bool acknowledgeBattle(GameState& state) {
  if (!isValidState(state) || (state.battle.status != BattleStatus::Won &&
      state.battle.status != BattleStatus::Lost)) return false;
  for (uint8_t i = 0; i < state.party.count; ++i) {
    auto& p = state.party.members[i];
    if (p.instanceId == state.battle.playerId) p.currentHp = calculateStats(p).hp;
  }
  state.battle = BattleState{};
  return true;
}
}
