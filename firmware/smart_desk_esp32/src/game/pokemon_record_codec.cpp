#include "pokemon_record_codec.h"

namespace PokemonGame {
namespace {
void put(uint8_t*& p, uint32_t value, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i) { *p++ = static_cast<uint8_t>(value); value >>= 8; }
}
uint32_t get(const uint8_t*& p, unsigned bytes) {
  uint32_t value = 0;
  for (unsigned i = 0; i < bytes; ++i) value |= static_cast<uint32_t>(*p++) << (8 * i);
  return value;
}
}
void encodePokemonRecord(const PokemonInstance& m, uint8_t* p) {
  put(p, m.instanceId, 4); put(p, m.speciesId, 2); put(p, m.exp, 4);
  put(p, m.currentHp, 2);
  for (MoveId move : m.moves) put(p, move, 2);
  put(p, m.level, 1); put(p, m.friendship, 1); put(p, m.formId, 1);
  put(p, static_cast<uint8_t>(m.gender) | (m.shiny ? 4u : 0u), 1);
}
bool decodePokemonRecord(const uint8_t* p, PokemonInstance& output) {
  PokemonInstance m;
  m.instanceId = get(p, 4); m.speciesId = static_cast<SpeciesId>(get(p, 2));
  m.exp = get(p, 4); m.currentHp = static_cast<uint16_t>(get(p, 2));
  for (auto& move : m.moves) move = static_cast<MoveId>(get(p, 2));
  m.level = static_cast<uint8_t>(get(p, 1));
  m.friendship = static_cast<uint8_t>(get(p, 1)); m.formId = static_cast<FormId>(get(p, 1));
  const uint8_t flags = static_cast<uint8_t>(get(p, 1));
  if (flags & 0xf8) return false;
  m.gender = static_cast<Gender>(flags & 3); m.shiny = (flags & 4) != 0;
  output = m;
  return true;
}
}
