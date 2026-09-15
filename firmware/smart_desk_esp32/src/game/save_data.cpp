#include "save_data.h"
#include <cstring>
#include <initializer_list>

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
uint32_t checksum(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; ++i) {
    if (i >= 14 && i < SAVE_HEADER_SIZE) continue;
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
  }
  return ~crc;
}
}

bool serialize(const GameSave& save, uint8_t* output, size_t capacity, size_t& written) {
  written = 0;
  if (!output || save.saveVersion != SAVE_VERSION || !isValidState(save.state)) return false;
  const size_t size = SAVE_HEADER_SIZE + 15 +
                      save.state.party.count * POKEMON_RECORD_SIZE + 3 * POKEDEX_BYTES;
  if (capacity < size) return false;
  uint8_t* p = output;
  for (char c : {'P', 'K', 'D', 'G'}) *p++ = static_cast<uint8_t>(c);
  put(p, save.saveVersion, 2);
  put(p, save.sequence, 4);
  put(p, static_cast<uint32_t>(size - SAVE_HEADER_SIZE), 4);
  put(p, 0, 4);
  const auto& progress = save.state.progress;
  put(p, progress.nextInstanceId, 4); put(p, progress.deskPetId, 4);
  put(p, progress.ballTier, 1); put(p, progress.masterBallCount, 1);
  put(p, progress.playTimeSeconds, 4);
  put(p, save.state.party.count, 1);
  for (uint8_t i = 0; i < save.state.party.count; ++i) {
    const auto& m = save.state.party.members[i];
    put(p, m.instanceId, 4); put(p, m.speciesId, 2); put(p, m.exp, 4);
    put(p, m.currentHp, 2);
    for (MoveId move : m.moves) put(p, move, 2);
    put(p, m.level, 1); put(p, m.friendship, 1); put(p, m.formId, 1);
    put(p, static_cast<uint8_t>(m.gender) | (m.shiny ? 4u : 0u), 1);
  }
  for (const uint8_t* bits : {save.state.pokedex.seen, save.state.pokedex.caught,
                             save.state.pokedex.shinyCaught})
    for (size_t i = 0; i < POKEDEX_BYTES; ++i) *p++ = bits[i];
  p = output + 14;
  put(p, checksum(output, size), 4);
  written = size;
  return true;
}

DecodeResult deserialize(const uint8_t* input, size_t length, GameSave& output) {
  if (!input || length < SAVE_HEADER_SIZE || std::memcmp(input, "PKDG", 4) != 0)
    return DecodeResult::Invalid;
  const uint8_t* p = input + 4;
  GameSave candidate;
  candidate.saveVersion = static_cast<uint16_t>(get(p, 2));
  candidate.sequence = get(p, 4);
  const uint32_t payloadLength = get(p, 4);
  const uint32_t crc = get(p, 4);
  if (payloadLength != length - SAVE_HEADER_SIZE || crc != checksum(input, length))
    return DecodeResult::Invalid;
  if (candidate.saveVersion != SAVE_VERSION) return DecodeResult::UnsupportedVersion;
  if (length < SAVE_HEADER_SIZE + 15 + 3 * POKEDEX_BYTES || length > SAVE_MAX_SIZE)
    return DecodeResult::Invalid;
  auto& progress = candidate.state.progress;
  progress.nextInstanceId = get(p, 4); progress.deskPetId = get(p, 4);
  progress.ballTier = static_cast<uint8_t>(get(p, 1));
  progress.masterBallCount = static_cast<uint8_t>(get(p, 1));
  progress.playTimeSeconds = get(p, 4);
  candidate.state.party.count = static_cast<uint8_t>(get(p, 1));
  if (candidate.state.party.count > PARTY_CAPACITY ||
      length != SAVE_HEADER_SIZE + 15 + candidate.state.party.count * POKEMON_RECORD_SIZE +
                3 * POKEDEX_BYTES) return DecodeResult::Invalid;
  for (uint8_t i = 0; i < candidate.state.party.count; ++i) {
    auto& m = candidate.state.party.members[i];
    m.instanceId = get(p, 4); m.speciesId = static_cast<SpeciesId>(get(p, 2));
    m.exp = get(p, 4); m.currentHp = static_cast<uint16_t>(get(p, 2));
    for (auto& move : m.moves) move = static_cast<MoveId>(get(p, 2));
    m.level = static_cast<uint8_t>(get(p, 1));
    m.friendship = static_cast<uint8_t>(get(p, 1)); m.formId = static_cast<FormId>(get(p, 1));
    const uint8_t flags = static_cast<uint8_t>(get(p, 1));
    if (flags & 0xf8) return DecodeResult::Invalid;
    m.gender = static_cast<Gender>(flags & 3); m.shiny = (flags & 4) != 0;
  }
  for (uint8_t* bits : {candidate.state.pokedex.seen, candidate.state.pokedex.caught,
                       candidate.state.pokedex.shinyCaught})
    for (size_t i = 0; i < POKEDEX_BYTES; ++i) bits[i] = *p++;
  if (!isValidState(candidate.state)) return DecodeResult::Invalid;
  output = candidate;
  return DecodeResult::Ok;
}
}
