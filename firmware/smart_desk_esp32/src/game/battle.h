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
bool resolveBattleTurn(GameState& state, uint8_t playerSlot);
bool acknowledgeBattle(GameState& state);
}
