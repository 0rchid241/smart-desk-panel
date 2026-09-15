#pragma once
#include "pokemon.h"
#include "exploration.h"
#include <cstddef>

namespace PokemonGame {
constexpr size_t PARTY_CAPACITY = 3;
constexpr SpeciesId POKEDEX_SPECIES_COUNT = 1025;
constexpr size_t POKEDEX_BYTES = (POKEDEX_SPECIES_COUNT + 7) / 8;

struct PartyState {
  PokemonInstance members[PARTY_CAPACITY] = {};
  uint8_t count = 0;
};
struct PokedexState {
  uint8_t seen[POKEDEX_BYTES] = {};
  uint8_t caught[POKEDEX_BYTES] = {};
  uint8_t shinyCaught[POKEDEX_BYTES] = {};
};
struct GameProgress {
  PokemonInstanceId nextInstanceId = 1;
  PokemonInstanceId deskPetId = 0; // G1 partner; resolved from the party.
  uint8_t ballTier = 0;
  uint8_t masterBallCount = 0;
  uint32_t playTimeSeconds = 0; // Reserved persistent field, no clock in G1.
};
struct GameState {
  GameProgress progress;
  PartyState party;
  PokedexState pokedex;
  ExplorationSession exploration;
};

bool dexContains(const uint8_t* bits, SpeciesId speciesId);
void registerCaught(PokedexState& dex, SpeciesId speciesId, bool shiny);
const PokemonInstance* partner(const GameState& state);
GameState createNewGame();
bool isValidState(const GameState& state);
}
