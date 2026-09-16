#pragma once
#include <cstddef>
#include "pokemon.h"

namespace PokemonGame {
constexpr size_t POKEMON_RECORD_SIZE = 24;
// Persistence-only wire codec. Callers validate Pokemon semantics separately.
void encodePokemonRecord(const PokemonInstance& pokemon, uint8_t* output);
bool decodePokemonRecord(const uint8_t* input, PokemonInstance& output);
}
