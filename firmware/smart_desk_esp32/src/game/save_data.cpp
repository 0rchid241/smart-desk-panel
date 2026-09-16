#include "save_data.h"
#include "crc32.h"
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
  const uint32_t crc = updateCrc32(0xffffffffu, data, 14);
  return ~updateCrc32(crc, data + SAVE_HEADER_SIZE, length - SAVE_HEADER_SIZE);
}
}

bool isValidBoxRoot(const BoxRoot& root) {
  return root.storeId != 0 && root.generation != 0 && root.capacity == BOX_CAPACITY &&
    root.occupiedCount <= BOX_CAPACITY && root.boxFormatVersion == BOX_FORMAT_VERSION && root.flags == 0;
}
BoxRoot boxRootFromMetadata(const BoxMetadata& metadata) {
  return {metadata.key.storeId, metadata.key.generation, BOX_CAPACITY,
          metadata.occupiedCount, metadata.crc32, BOX_FORMAT_VERSION, 0};
}
bool serialize(const GameSave& save, uint8_t* output, size_t capacity, size_t& written) {
  written = 0;
  if (!output || save.saveVersion != SAVE_VERSION || !isValidState(save.state) ||
      !isValidBoxRoot(save.boxRoot)) return false;
  const size_t size =
    SAVE_HEADER_SIZE + 15 +
    save.state.party.count * POKEMON_RECORD_SIZE +
    3 * POKEDEX_BYTES +
    EXPLORATION_RECORD_SIZE +
    ENCOUNTER_RECORD_SIZE + BATTLE_RECORD_SIZE + BOX_ROOT_RECORD_SIZE;
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
    encodePokemonRecord(save.state.party.members[i], p);
    p += POKEMON_RECORD_SIZE;
  }
  for (const uint8_t* bits : {save.state.pokedex.seen, save.state.pokedex.caught,
                             save.state.pokedex.shinyCaught})
    for (size_t i = 0; i < POKEDEX_BYTES; ++i) *p++ = bits[i];
  const auto& session = save.state.exploration;
  put(p, static_cast<uint8_t>(session.status), 1);
  put(p, session.regionId, 2);
  put(p, static_cast<uint32_t>(session.startedAtEpoch), 4);
  put(p, static_cast<uint32_t>(session.startedAtEpoch >> 32), 4);
  put(p, session.durationSeconds, 4);
  const auto& encounter = save.state.encounter;
  put(p, static_cast<uint8_t>(encounter.status), 1);
  put(p, encounter.speciesId, 2);
  put(p, encounter.formId, 1);
  put(p, encounter.level, 1);
  put(p, static_cast<uint8_t>(encounter.gender), 1);
  put(p, encounter.shiny ? 1u : 0u, 1);
  const auto& b = save.state.battle;
  put(p, static_cast<uint8_t>(b.status), 1); put(p, b.playerId, 4);
  put(p, static_cast<uint8_t>(b.wild.status), 1); put(p, b.wild.speciesId, 2);
  put(p, b.wild.formId, 1); put(p, b.wild.level, 1);
  put(p, static_cast<uint8_t>(b.wild.gender), 1); put(p, b.wild.shiny ? 1u : 0u, 1);
  for (auto id : b.wildMoves) put(p, id, 2);
  for (const auto* c : {&b.player, &b.opponent}) {
    put(p, c->currentHp, 2);
    for (auto pp : c->pp) put(p, pp, 1);
  }
  put(p, b.turn, 4); put(p, b.rngState, 4);
  const auto& root = save.boxRoot;
  put(p, static_cast<uint32_t>(root.storeId), 4); put(p, static_cast<uint32_t>(root.storeId >> 32), 4);
  put(p, static_cast<uint32_t>(root.generation), 4); put(p, static_cast<uint32_t>(root.generation >> 32), 4);
  put(p, root.capacity, 4); put(p, root.occupiedCount, 4); put(p, root.snapshotCrc32, 4);
  put(p, root.boxFormatVersion, 2); put(p, root.flags, 2);
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
  if (
    candidate.saveVersion != 1 &&
    candidate.saveVersion != 2 &&
    candidate.saveVersion != 3 &&
    candidate.saveVersion != 4 &&
    candidate.saveVersion != 5)
    return DecodeResult::UnsupportedVersion;
  const size_t explorationBytes =
    candidate.saveVersion >= 2
    ? EXPLORATION_RECORD_SIZE
    : 0;
  const size_t encounterBytes =
    candidate.saveVersion >= 3
    ? ENCOUNTER_RECORD_SIZE
    : 0;
  const size_t battleBytes = candidate.saveVersion >= 4 ? BATTLE_RECORD_SIZE : 0;
  const size_t rootBytes = candidate.saveVersion >= 5 ? BOX_ROOT_RECORD_SIZE : 0;
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
                3 * POKEDEX_BYTES + explorationBytes + encounterBytes + battleBytes + rootBytes) return DecodeResult::Invalid;
  for (uint8_t i = 0; i < candidate.state.party.count; ++i) {
    if (!decodePokemonRecord(p, candidate.state.party.members[i])) return DecodeResult::Invalid;
    p += POKEMON_RECORD_SIZE;
  }
  for (uint8_t* bits : {candidate.state.pokedex.seen, candidate.state.pokedex.caught,
                       candidate.state.pokedex.shinyCaught})
    for (size_t i = 0; i < POKEDEX_BYTES; ++i) bits[i] = *p++;
  if (candidate.saveVersion >= 2) {
    auto& session = candidate.state.exploration;
    session.status = static_cast<ExplorationStatus>(get(p, 1));
    session.regionId = static_cast<uint16_t>(get(p, 2));
    session.startedAtEpoch = get(p, 4);
    session.startedAtEpoch |= static_cast<uint64_t>(get(p, 4)) << 32;
    session.durationSeconds = get(p, 4);
  }
  if (candidate.saveVersion >= 3) {
    auto& encounter = candidate.state.encounter;
    encounter.status = static_cast<EncounterStatus>(get(p, 1));
    encounter.speciesId = static_cast<SpeciesId>(get(p, 2));
    encounter.formId = static_cast<FormId>(get(p, 1));
    encounter.level = static_cast<uint8_t>(get(p, 1));
    encounter.gender = static_cast<Gender>(get(p, 1));
    const auto shiny = get(p, 1);
    if (shiny > 1) return DecodeResult::Invalid;
    encounter.shiny = shiny != 0;
  }
  if (candidate.saveVersion >= 4) {
    auto& b = candidate.state.battle;
    b.status = static_cast<BattleStatus>(get(p, 1)); b.playerId = get(p, 4);
    b.wild.status = static_cast<EncounterStatus>(get(p, 1));
    b.wild.speciesId = static_cast<SpeciesId>(get(p, 2));
    b.wild.formId = static_cast<FormId>(get(p, 1));
    b.wild.level = static_cast<uint8_t>(get(p, 1));
    b.wild.gender = static_cast<Gender>(get(p, 1));
    const auto shiny = get(p, 1);
    if (shiny > 1) return DecodeResult::Invalid;
    b.wild.shiny = shiny != 0;
    for (auto& id : b.wildMoves) id = static_cast<MoveId>(get(p, 2));
    for (auto* c : {&b.player, &b.opponent}) {
      c->currentHp = static_cast<uint16_t>(get(p, 2));
      for (auto& pp : c->pp) pp = static_cast<uint8_t>(get(p, 1));
    }
    b.turn = get(p, 4); b.rngState = get(p, 4);
  }
  if (candidate.saveVersion >= 5) {
    auto& root = candidate.boxRoot;
    root.storeId = get(p, 4); root.storeId |= static_cast<uint64_t>(get(p, 4)) << 32;
    root.generation = get(p, 4); root.generation |= static_cast<uint64_t>(get(p, 4)) << 32;
    root.capacity = get(p, 4); root.occupiedCount = get(p, 4); root.snapshotCrc32 = get(p, 4);
    root.boxFormatVersion = static_cast<uint16_t>(get(p, 2));
    root.flags = static_cast<uint16_t>(get(p, 2));
    if (!isValidBoxRoot(root)) return DecodeResult::Invalid;
  }
  if (!isValidState(candidate.state)) return DecodeResult::Invalid;
  output = candidate;
  return DecodeResult::Ok;
}
}
