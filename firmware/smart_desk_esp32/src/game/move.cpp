#include "move.h"

namespace PokemonGame {
namespace {
const MoveData moves[] = {
  {21, "힘껏치기", Type::Normal, MoveCategory::Physical, 80, 75, 20},
  {33, "몸통박치기", Type::Normal, MoveCategory::Physical, 40, 100, 35},
  {45, "울음소리", Type::Normal, MoveCategory::Status, 0, 100, 40},
  {84, "전기쇼크", Type::Electric, MoveCategory::Special, 40, 100, 30}
};
const MoveData struggle = {0, "발버둥", Type::None, MoveCategory::Physical, 50, 100, 0};
}
const MoveData* findMove(MoveId id) {
  for (const auto& move : moves) if (move.id == id) return &move;
  return nullptr;
}
const MoveData& struggleMove() { return struggle; }
void fixtureMoves(SpeciesId species, MoveId (&result)[4]) {
  for (auto& id : result) id = 0;
  if (findSpecies(species)) {
    result[0] = species == 25 ? 84 : 33;
    result[1] = 45;
  }
}
}
