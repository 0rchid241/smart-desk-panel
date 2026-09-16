#pragma once
#include <cstdint>
#include "type.h"

namespace PokemonGame {
using SpeciesId = uint16_t;
using MoveId = uint16_t;
using PokemonInstanceId = uint32_t;
using FormId = uint8_t;
enum class Gender : uint8_t { Male, Female, Genderless };

struct Stats {
  uint16_t hp, attack, defense, spAttack, spDefense, speed;
};

// 공개 Vertical Slice 종 fixture. 전체 콘텐츠/learnset 테이블은 아직 없다.
struct PokemonSpecies {
  SpeciesId speciesId;
  const char* name;
  Stats baseStats;
  Type primaryType;
  Type secondaryType;
};

struct PokemonInstance {
  PokemonInstanceId instanceId = 0;
  SpeciesId speciesId = 0;
  uint32_t exp = 0; // G1: progress within the current level; no leveling yet.
  uint16_t currentHp = 0;
  MoveId moves[4] = {};
  uint8_t level = 1;
  uint8_t friendship = 0;
  FormId formId = 0;
  Gender gender = Gender::Male;
  bool shiny = false;
};

const PokemonSpecies* findSpecies(SpeciesId speciesId, FormId formId = 0);
// No IV, EV or nature terms. Invalid species/form/level returns zero stats.
Stats calculateStats(const PokemonInstance& pokemon);
bool isValidPokemon(const PokemonInstance& pokemon);
}
