#pragma once
#include "pokemon.h"

namespace PokemonGame {
enum class MoveCategory : uint8_t { Physical, Special, Status };
struct MoveData {
  MoveId id;
  const char* name;
  Type type;
  MoveCategory category;
  uint16_t power;
  uint8_t accuracy, maxPP;
};
const MoveData* findMove(MoveId id);
// 모든 PP 소진 시에만 사용하는 무속성 공격. 영구 기술 ID로 저장하지 않는다.
const MoveData& struggleMove();
void fixtureMoves(SpeciesId species, MoveId (&moves)[4]);
}
