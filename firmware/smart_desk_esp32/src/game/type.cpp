#include "type.h"
#include <initializer_list>

namespace PokemonGame {
bool isValidType(Type type) { return static_cast<uint8_t>(type) <= 18; }
namespace {
bool contains(Type type, std::initializer_list<Type> types) {
  for (Type item : types) if (item == type) return true;
  return false;
}
uint8_t single(Type a, Type d) {
  using T = Type;
  if (d == T::None) return 4;
  // 각 행은 2배 / 반감 / 무효 순서다. 그 외는 1배.
  // initializer_list의 수명은 아래 각 switch 분기 안에서만 사용한다.
  auto row = [d](std::initializer_list<Type> s, std::initializer_list<Type> w,
                 std::initializer_list<Type> z) -> uint8_t {
    return contains(d, z) ? 0 : contains(d, s) ? 8 : contains(d, w) ? 2 : 4;
  };
  switch (a) {
    case T::Normal: return row({}, {T::Rock,T::Steel}, {T::Ghost});
    case T::Fire: return row({T::Grass,T::Ice,T::Bug,T::Steel}, {T::Fire,T::Water,T::Rock,T::Dragon}, {});
    case T::Water: return row({T::Fire,T::Ground,T::Rock}, {T::Water,T::Grass,T::Dragon}, {});
    case T::Electric: return row({T::Water,T::Flying}, {T::Electric,T::Grass,T::Dragon}, {T::Ground});
    case T::Grass: return row({T::Water,T::Ground,T::Rock}, {T::Fire,T::Grass,T::Poison,T::Flying,T::Bug,T::Dragon,T::Steel}, {});
    case T::Ice: return row({T::Grass,T::Ground,T::Flying,T::Dragon}, {T::Fire,T::Water,T::Ice,T::Steel}, {});
    case T::Fighting: return row({T::Normal,T::Ice,T::Rock,T::Dark,T::Steel}, {T::Poison,T::Flying,T::Psychic,T::Bug,T::Fairy}, {T::Ghost});
    case T::Poison: return row({T::Grass,T::Fairy}, {T::Poison,T::Ground,T::Rock,T::Ghost}, {T::Steel});
    case T::Ground: return row({T::Fire,T::Electric,T::Poison,T::Rock,T::Steel}, {T::Grass,T::Bug}, {T::Flying});
    case T::Flying: return row({T::Grass,T::Fighting,T::Bug}, {T::Electric,T::Rock,T::Steel}, {});
    case T::Psychic: return row({T::Fighting,T::Poison}, {T::Psychic,T::Steel}, {T::Dark});
    case T::Bug: return row({T::Grass,T::Psychic,T::Dark}, {T::Fire,T::Fighting,T::Poison,T::Flying,T::Ghost,T::Steel,T::Fairy}, {});
    case T::Rock: return row({T::Fire,T::Ice,T::Flying,T::Bug}, {T::Fighting,T::Ground,T::Steel}, {});
    case T::Ghost: return row({T::Psychic,T::Ghost}, {T::Dark}, {T::Normal});
    case T::Dragon: return row({T::Dragon}, {T::Steel}, {T::Fairy});
    case T::Dark: return row({T::Psychic,T::Ghost}, {T::Fighting,T::Dark,T::Fairy}, {});
    case T::Steel: return row({T::Ice,T::Rock,T::Fairy}, {T::Fire,T::Water,T::Electric,T::Steel}, {});
    case T::Fairy: return row({T::Fighting,T::Dragon,T::Dark}, {T::Fire,T::Poison,T::Steel}, {});
    default: return 0;
  }
}
}
uint8_t typeEffectiveness(Type a, Type p, Type s) {
  if (!isValidType(a) || !isValidType(p) || !isValidType(s) ||
      a == Type::None || p == Type::None || p == s) return 0;
  return static_cast<uint8_t>(single(a, p) * single(a, s) / 4);
}
}
