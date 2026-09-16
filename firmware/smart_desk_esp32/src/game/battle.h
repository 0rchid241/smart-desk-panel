#pragma once
#include "encounter.h"
#include "move.h"

namespace PokemonGame {
struct GameState;
enum class BattleStatus : uint8_t { None, Active, Won, Lost };
struct BattleCombatant {
  uint16_t currentHp = 0;
  uint8_t pp[4] = {};
};
struct BattleState {
  BattleStatus status = BattleStatus::None;
  PokemonInstanceId playerId = 0;
  // Ready는 종/폼/레벨/성별을 나타낸다. GameState::encounter와는 별개다.
  EncounterState wild;
  MoveId wildMoves[4] = {};
  BattleCombatant player, opponent;
  uint32_t turn = 0;
  uint32_t rngState = 0;
};
constexpr uint8_t STRUGGLE_SLOT = 4;
// 화면용 일시 보고서. BattleState/세이브에는 포함하지 않는다.
enum class BattleActor : uint8_t { Player, Wild };
struct BattleActionReport {
  BattleActor actor = BattleActor::Player;
  MoveId moveId = 0; // 0은 발버둥.
  bool hit = false;
  uint16_t damage = 0; // overkill을 제외한 실제 HP 감소량.
  uint8_t effectiveness = 4; // 4 = 1배.
  bool fainted = false;
};
struct BattleTurnReport {
  BattleActionReport actions[2] = {};
  uint8_t count = 0;
};
PokemonInstance wildPokemon(const BattleState& battle);
bool isValidBattle(const BattleState& battle, const PokemonInstance* player);
uint32_t battleRandom(uint32_t& state);
bool moveHits(uint8_t accuracy, uint32_t& rng);
uint16_t battleDamage(const PokemonSpecies& attacker, uint8_t level, const Stats& attackStats,
                      const PokemonSpecies& defender, const Stats& defenseStats,
                      const MoveData& move, uint8_t variation);
bool canSelectMove(const PokemonInstance& player, const BattleState& battle, uint8_t slot);
uint8_t nextBattleMove(const PokemonInstance& player, const BattleState& battle,
                       uint8_t current, int direction);
bool startBattle(GameState& state);
// 실패 시 state와 report 모두 유지한다. 저장 성공 여부는 호출자가 결정한다.
bool resolveBattleTurn(GameState& state, uint8_t playerSlot, BattleTurnReport* report = nullptr);
bool acknowledgeBattle(GameState& state);
}
