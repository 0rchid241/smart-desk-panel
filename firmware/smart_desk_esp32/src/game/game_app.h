#pragma once
#include "../core/app_types.h"
#include "game_state.h"

namespace GameApp {
// Animation/menu state is private; mode routing is owned by the sketch.
void init();
// Runs in both device modes. Updates persistence only, never draws Desk UI.
void update();
void drawDeskPet();
void drawGameGraphics();
void drawGameTextScreen();
void updatePokemonAnimation(DeviceMode deviceMode);
void handleButton(ButtonEvent button);
// Read-only access; changes are validated, persisted, then applied atomically.
const PokemonGame::GameState& state();
bool saveState(const PokemonGame::GameState& next);
}
