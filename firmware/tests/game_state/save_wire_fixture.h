#pragma once
#include "save_data.h"

inline PokemonGame::GameSave legacyWireInput() {
  using namespace PokemonGame;
  GameSave save;
  save.state = createNewGame();
  save.sequence = 0x12345678;
  save.state.party.count = 3;
  for (unsigned i = 0; i < 3; ++i) {
    save.state.party.members[i] = save.state.party.members[0];
    auto& m = save.state.party.members[i];
    m.instanceId = i + 1;
    m.level = static_cast<uint8_t>(5 + i);
    m.exp = 0x12340000u + i;
    m.currentHp = static_cast<uint16_t>(i);
    m.gender = static_cast<Gender>(i);
  }
  save.state.party.members[1].shiny = true;
  registerCaught(save.state.pokedex, 25, true);
  registerCaught(save.state.pokedex, 1025, true);
  save.state.progress.nextInstanceId = 4;
  save.state.progress.deskPetId = 2;
  save.state.progress.ballTier = 2;
  save.state.progress.masterBallCount = 7;
  save.state.progress.playTimeSeconds = 987654;
  return save;
}
