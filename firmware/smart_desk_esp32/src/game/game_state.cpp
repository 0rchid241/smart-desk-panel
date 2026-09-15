#include "game_state.h"

namespace PokemonGame {
bool dexContains(const uint8_t* bits, SpeciesId id) {
  if (!bits || id == 0 || id > POKEDEX_SPECIES_COUNT) return false;
  return (bits[(id - 1) / 8] & (1u << ((id - 1) % 8))) != 0;
}

void registerCaught(PokedexState& dex, SpeciesId id, bool shiny) {
  if (id == 0 || id > POKEDEX_SPECIES_COUNT) return;
  const size_t index = (id - 1) / 8;
  const uint8_t mask = static_cast<uint8_t>(1u << ((id - 1) % 8));
  dex.seen[index] |= mask;
  dex.caught[index] |= mask;
  if (shiny) dex.shinyCaught[index] |= mask;
}

const PokemonInstance* partner(const GameState& state) {
  if (state.party.count > PARTY_CAPACITY) return nullptr;
  for (uint8_t i = 0; i < state.party.count; ++i)
    if (state.party.members[i].instanceId == state.progress.deskPetId)
      return &state.party.members[i];
  return nullptr;
}

GameState createNewGame() {
  GameState state;
  auto& p = state.party.members[0];
  p.instanceId = state.progress.nextInstanceId++;
  p.speciesId = 25;
  p.level = 5;
  p.exp = 0;
  p.currentHp = calculateStats(p).hp;
  p.friendship = 70; // Vertical Slice fixture, not the final starter selection.
  p.moves[0] = 84;
  p.moves[1] = 45;
  state.party.count = 1;
  state.progress.deskPetId = p.instanceId;
  registerCaught(state.pokedex, p.speciesId, p.shiny);
  return state;
}

bool isValidState(const GameState& state) {
  if (!isValidExploration(state.exploration)) return false;
  if (state.party.count == 0 || state.party.count > PARTY_CAPACITY ||
      !state.progress.nextInstanceId || state.progress.ballTier > 2 ||
      !partner(state)) return false;
  for (uint8_t i = 0; i < state.party.count; ++i) {
    const auto& p = state.party.members[i];
    if (!isValidPokemon(p) || p.instanceId >= state.progress.nextInstanceId ||
        !dexContains(state.pokedex.caught, p.speciesId) ||
        (p.shiny && !dexContains(state.pokedex.shinyCaught, p.speciesId))) return false;
    for (uint8_t j = 0; j < i; ++j)
      if (p.instanceId == state.party.members[j].instanceId) return false;
  }
  for (size_t i = 0; i < POKEDEX_BYTES; ++i) {
    if ((state.pokedex.caught[i] & ~state.pokedex.seen[i]) ||
        (state.pokedex.shinyCaught[i] & ~state.pokedex.caught[i])) return false;
  }
  // Only bit 0 of the final byte represents a species (1025).
  return ((state.pokedex.seen[128] | state.pokedex.caught[128] |
           state.pokedex.shinyCaught[128]) & 0xfe) == 0;
}
}
