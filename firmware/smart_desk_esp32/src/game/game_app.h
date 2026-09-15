#pragma once
#include "../core/app_types.h"

namespace GameApp {
// Animation/menu state is private; mode routing is owned by the sketch.
void init();
void drawDeskPet();
void drawGameGraphics();
void drawGameTextScreen();
void updatePokemonAnimation(DeviceMode deviceMode);
void handleButton(ButtonEvent button);
}
