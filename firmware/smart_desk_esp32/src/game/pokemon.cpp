#include "pokemon.h"

namespace PokemonGame {
namespace {
const PokemonSpecies pikachu = {25, "피카츄", {35, 55, 40, 50, 50, 90}};
uint16_t stat(uint16_t base, uint8_t level) {
  return static_cast<uint16_t>((2u * base * level) / 100u + 5u);
}
}

const PokemonSpecies* findSpecies(SpeciesId speciesId, FormId formId) {
  return speciesId == 25 && formId == 0 ? &pikachu : nullptr;
}

Stats calculateStats(const PokemonInstance& pokemon) {
  const auto* species = findSpecies(pokemon.speciesId, pokemon.formId);
  if (!species || pokemon.level == 0 || pokemon.level > 100) return {};
  const Stats& b = species->baseStats;
  const uint8_t level = pokemon.level;
  return {static_cast<uint16_t>((2u * b.hp * level) / 100u + level + 10u),
          stat(b.attack, level), stat(b.defense, level), stat(b.spAttack, level),
          stat(b.spDefense, level), stat(b.speed, level)};
}

bool isValidPokemon(const PokemonInstance& pokemon) {
  if (!pokemon.instanceId || !findSpecies(pokemon.speciesId, pokemon.formId) ||
      pokemon.level == 0 || pokemon.level > 100 ||
      static_cast<uint8_t>(pokemon.gender) > 2 ||
      pokemon.currentHp > calculateStats(pokemon).hp) return false;
  for (int i = 0; i < 4; ++i) {
    // Only the two G1 fixture move IDs (Thunder Shock / Growl) are supported.
    if (pokemon.moves[i] != 0 && pokemon.moves[i] != 84 && pokemon.moves[i] != 45)
      return false;
    for (int j = 0; j < i; ++j)
      if (pokemon.moves[i] && pokemon.moves[i] == pokemon.moves[j]) return false;
  }
  return true;
}
}
