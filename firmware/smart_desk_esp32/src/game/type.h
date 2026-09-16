#pragma once
#include <cstdint>

namespace PokemonGame {
enum class Type : uint8_t {
  None, Normal, Fire, Water, Electric, Grass, Ice, Fighting, Poison,
  Ground, Flying, Psychic, Bug, Rock, Ghost, Dragon, Dark, Steel, Fairy
};
bool isValidType(Type type);
// 4 = 1배. 잘못된 타입은 0, 두 번째 None은 단일 타입이다.
uint8_t typeEffectiveness(Type attack, Type primary, Type secondary = Type::None);
}
