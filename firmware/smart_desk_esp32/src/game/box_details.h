#pragma once
#include "pokemon.h"
#include "move.h"

// Read-only presentation helpers; zero slots are omitted, unknown IDs stay visible.
namespace BoxDetails {
inline uint8_t moveCount(const PokemonGame::PokemonInstance& pokemon) {
  uint8_t count = 0;
  for (auto id : pokemon.moves) if (id) ++count;
  return count;
}
inline uint8_t movePages(const PokemonGame::PokemonInstance& pokemon) {
  const uint8_t count = moveCount(pokemon);
  return count > 2 ? 2 : 1;
}
inline const char* moveName(const PokemonGame::PokemonInstance& pokemon, uint8_t index) {
  for (auto id : pokemon.moves) {
    if (!id) continue;
    if (index) { --index; continue; }
    const auto* move = PokemonGame::findMove(id);
    return move ? move->name : "?";
  }
  return nullptr;
}
}
