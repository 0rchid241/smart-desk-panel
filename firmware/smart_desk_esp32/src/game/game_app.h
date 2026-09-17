#pragma once
#include "../core/app_types.h"
#include "game_state.h"

namespace GameApp {
// Animation/menu state is private; mode routing is owned by the sketch.
void init();
bool boxBrowserActive();
// Close transient Box handles on a mode change; other game screens stay intact.
void leaveGameMode();
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
