#include "pokemon.h"
#include "move.h"

namespace PokemonGame {
namespace {

const PokemonSpecies pidgey = {
  16,
  "구구",
  {40, 45, 40, 35, 35, 56}, Type::Normal, Type::Flying
};

const PokemonSpecies rattata = {
  19,
  "꼬렛",
  {30, 56, 35, 25, 35, 72}, Type::Normal, Type::None
};

const PokemonSpecies pikachu = {
  25,
  "피카츄",
  {35, 55, 40, 50, 50, 90}, Type::Electric, Type::None
};

uint16_t stat(uint16_t base, uint8_t level) {
  return static_cast<uint16_t>((2u * base * level) / 100u + 5u);
}
}

const PokemonSpecies* findSpecies(
  SpeciesId speciesId,
  FormId formId
) {
  if (formId != 0) {
    return nullptr;
  }

  switch (speciesId) {
    case 16:
      return &pidgey;

    case 19:
      return &rattata;

    case 25:
      return &pikachu;

    default:
      return nullptr;
  }
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
    if (pokemon.moves[i] != 0 && !findMove(pokemon.moves[i]))
      return false;
    for (int j = 0; j < i; ++j)
      if (pokemon.moves[i] && pokemon.moves[i] == pokemon.moves[j]) return false;
  }
  return true;
}
}
