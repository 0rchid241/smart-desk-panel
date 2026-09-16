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

uint8_t chooseWildMove(BattleState& b, const PokemonInstance& wild) {
  uint8_t options[4] = {};
  uint8_t count = 0;

  for (uint8_t i = 0; i < 4; ++i) {
    const auto* m = findMove(wild.moves[i]);
    if (m && m->category != MoveCategory::Status && b.opponent.pp[i]) {
      options[count++] = i;
    }
  }

  // 공격 PP가 없으면 남은 변화 기술도 사용한다.
  if (!count) {
    for (uint8_t i = 0; i < 4; ++i) {
      if (findMove(wild.moves[i]) && b.opponent.pp[i]) {
        options[count++] = i;
      }
    }
  }

  if (!count) {
    return STRUGGLE_SLOT;
  }

  return options[battleRandom(b.rngState) % count];
}

void resolveWildCounterattack(
  BattleState& b,
  const PokemonInstance& player,
  const PokemonInstance& wild,
  BattleActionReport& action
) {
  const uint8_t selected =
    chooseWildMove(
      b,
      wild
    );

  const auto& move =
    selected == STRUGGLE_SLOT
      ? struggleMove()
      : *findMove(
          wild.moves[
            selected
          ]
        );

  action.actor = BattleActor::Wild;
  action.moveId = move.id;

  const auto* playerSpecies =
    findSpecies(
      player.speciesId,
      player.formId
    );

  action.effectiveness =
    move.type == Type::None
      ? 4
      : typeEffectiveness(
          move.type,
          playerSpecies->primaryType,
          playerSpecies->secondaryType
        );

  if (
    selected < 4
  ) {
    --b.opponent.pp[
      selected
    ];
  }

  action.hit =
    moveHits(
      move.accuracy,
      b.rngState
    );

  if (
    !action.hit ||
    move.category ==
      MoveCategory::Status
  ) {
    return;
  }

  const auto playerStats =
    calculateStats(
      player
    );

  const auto wildStats =
    calculateStats(
      wild
    );

  const auto variation =
    static_cast<uint8_t>(
      85 +
      battleRandom(
        b.rngState
      ) %
      16
    );

  const uint16_t damage =
    battleDamage(
      *findSpecies(
        wild.speciesId,
        wild.formId
      ),
      wild.level,
      wildStats,
      *playerSpecies,
      playerStats,
      move,
      variation
    );

  action.damage =
    damage >=
      b.player.currentHp
      ? b.player.currentHp
      : damage;

  b.player.currentHp =
    static_cast<uint16_t>(
      b.player.currentHp -
      action.damage
    );

  action.fainted =
    b.player.currentHp ==
    0;
}

uint8_t captureBaseChance(SpeciesId speciesId) {
  switch (speciesId) {
    case 16:
      return 55; // 구구
    case 19:
      return 60; // 꼬렛
    case 25:
      return 45; // 피카츄
    default:
      return 0;
  }
}

uint16_t captureBallMultiplier(CaptureBall ball) {
  switch (ball) {
    case CaptureBall::Poke:
      return 100;
    case CaptureBall::Great:
      return 130;
    case CaptureBall::Ultra:
      return 160;
    case CaptureBall::Master:
      return 100;
    default:
      return 0;
  }
}
} // namespace

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

bool attemptBattleRun(GameState& state, BattleRunReport* report) {
  if (
    !isValidState(
      state
    ) ||
    state.battle.status !=
      BattleStatus::Active
  ) {
    return false;
  }

  auto next =
    state;

  auto& b =
    next.battle;

  BattleRunReport runReport;

  const auto& player =
    *partner(
      next
    );

  const auto wild =
    wildPokemon(
      b
    );

  const auto playerStats =
    calculateStats(
      player
    );

  const auto wildStats =
    calculateStats(
      wild
    );

  // G5-A2 단순 도주식:
  // 기본 60%, 속도 차이 1마다 2% 보정, 최종 25~90%로 제한한다.
  // 시도 횟수는 저장하지 않아 SAVE_VERSION=4를 그대로 유지한다.
  int chance =
    60 +
    (
      static_cast<int>(
        playerStats.speed
      ) -
      static_cast<int>(
        wildStats.speed
      )
    ) *
    2;

  if (
    chance < 25
  ) {
    chance = 25;
  } else if (
    chance > 90
  ) {
    chance = 90;
  }

  const uint32_t roll =
    battleRandom(
      b.rngState
    ) %
    100u;

  runReport.escaped =
    roll <
    static_cast<uint32_t>(
      chance
    );

  if (
    runReport.escaped
  ) {
    b =
      BattleState{};

    if (
      !isValidState(
        next
      )
    ) {
      return false;
    }

    state =
      next;

    if (
      report
    ) {
      *report =
        runReport;
    }

    return true;
  }

  runReport.opponentActed =
    true;

  resolveWildCounterattack(
    b,
    player,
    wild,
    runReport.opponentAction
  );

  if (
    b.turn !=
    UINT32_MAX
  ) {
    ++b.turn;
  }

  if (
    !b.player.currentHp
  ) {
    b.status =
      BattleStatus::Lost;
  }

  if (
    !isValidState(
      next
    )
  ) {
    return false;
  }

  state =
    next;

  if (
    report
  ) {
    *report =
      runReport;
  }

  return true;
}

bool canUseCaptureBall(const GameState& state, CaptureBall ball) {
  if (state.progress.nextInstanceId == UINT32_MAX)
    return false;
  if (
    !isValidState(
      state
    ) ||
    state.battle.status !=
      BattleStatus::Active
  ) {
    return false;
  }

  switch (ball) {
    case CaptureBall::Poke:
      return true;

    case CaptureBall::Great:
      return
        state.progress.ballTier >=
        1;

    case CaptureBall::Ultra:
      return
        state.progress.ballTier >=
        2;

    case CaptureBall::Master:
      return
        state.progress.masterBallCount >
        0;

    default:
      return false;
  }
}

uint8_t captureChance(const GameState& state, CaptureBall ball) {
  if (
    !canUseCaptureBall(
      state,
      ball
    )
  ) {
    return 0;
  }

  if (
    ball ==
    CaptureBall::Master
  ) {
    return 100;
  }

  const auto& b =
    state.battle;

  const auto wild =
    wildPokemon(
      b
    );

  const uint16_t maxHp =
    calculateStats(
      wild
    ).hp;

  if (
    !maxHp ||
    b.opponent.currentHp >
      maxHp
  ) {
    return 0;
  }

  const uint8_t base =
    captureBaseChance(
      b.wild.speciesId
    );

  if (
    !base
  ) {
    return 0;
  }

  const uint32_t missingPercent =
    static_cast<uint32_t>(
      maxHp -
      b.opponent.currentHp
    ) *
    100u /
    maxHp;

  // G5-B 단순 포획식:
  // 종별 기본 확률 + 잃은 HP 비율의 절반에 볼 배율을 적용한다.
  // 상태이상 보정은 상태 시스템이 생기는 단계에서 추가한다.
  uint32_t chance =
    (
      static_cast<uint32_t>(
        base
      ) +
      missingPercent /
      2u
    ) *
    captureBallMultiplier(
      ball
    ) /
    100u;

  if (
    chance < 5u
  ) {
    chance = 5u;
  } else if (
    chance > 95u
  ) {
    chance = 95u;
  }

  return
    static_cast<uint8_t>(
      chance
    );
}

bool attemptBattleCapture(
  GameState& state,
  CaptureBall ball,
  BattleCaptureReport* report,
  CaptureDestination destination
) {
  if ((destination != CaptureDestination::Party && destination != CaptureDestination::Box) ||
      (destination == CaptureDestination::Party && state.party.count >= PARTY_CAPACITY)) return false;
  if (
    !canUseCaptureBall(
      state,
      ball
    )
  ) {
    return false;
  }

  auto next =
    state;

  auto& b =
    next.battle;

  BattleCaptureReport captureReport;
  captureReport.destination = destination;
  captureReport.ball =
    ball;
  captureReport.wild =
    b.wild;
  captureReport.chance =
    captureChance(
      next,
      ball
    );

  if (
    ball ==
    CaptureBall::Master
  ) {
    --next.progress.masterBallCount;
    captureReport.captured =
      true;
  } else {
    const uint32_t roll =
      battleRandom(
        b.rngState
      ) %
      100u;

    captureReport.captured =
      roll <
      captureReport.chance;
  }

  if (
    captureReport.captured
  ) {
    // 계산용 wildPokemon의 임시 ID를 쓰지 않고 영구 소유 ID를 발급한다.
    PokemonInstance caught;
    caught.instanceId = next.progress.nextInstanceId++;
    caught.speciesId = b.wild.speciesId;
    caught.formId = b.wild.formId;
    caught.level = b.wild.level;
    caught.gender = b.wild.gender;
    caught.shiny = b.wild.shiny;
    caught.currentHp = b.opponent.currentHp;
    caught.exp = 0;
    caught.friendship = 0; // C1 fixture: 종별 초기 친밀도는 후속 단계에서 정의한다.
    for (uint8_t i = 0; i < 4; ++i) caught.moves[i] = b.wildMoves[i];
    captureReport.caught = caught;
    if (destination == CaptureDestination::Party) next.party.members[next.party.count++] = caught;
    registerCaught(next.pokedex, caught.speciesId, caught.shiny);

    // 소유/도감/볼 소비와 전투 종료가 같은 후보 상태에 포함된다.
    b =
      BattleState{};

    if (
      !isValidState(
        next
      )
    ) {
      return false;
    }

    state =
      next;

    if (
      report
    ) {
      *report =
        captureReport;
    }

    return true;
  }

  const auto& player =
    *partner(
      next
    );

  const auto wild =
    wildPokemon(
      b
    );

  captureReport.opponentActed =
    true;

  resolveWildCounterattack(
    b,
    player,
    wild,
    captureReport.opponentAction
  );

  if (
    b.turn !=
    UINT32_MAX
  ) {
    ++b.turn;
  }

  if (
    !b.player.currentHp
  ) {
    b.status =
      BattleStatus::Lost;
  }

  if (
    !isValidState(
      next
    )
  ) {
    return false;
  }

  state =
    next;

  if (
    report
  ) {
    *report =
      captureReport;
  }

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
