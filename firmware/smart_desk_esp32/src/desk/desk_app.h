#pragma once
#include "../core/app_types.h"

namespace DeskApp {
void init();
void showMessage(const char* line1, const char* line2 = "");
// Timer update runs in both modes; render() owns the Desk view on OLED 1.
void update();
void render();
void handleButton(ButtonEvent button);
bool notificationActive();
void dismissNotification();
void triggerNotification(const String& title, const String& message);
// The entry point lends OLED 2 to Desk notifications while in GAME mode.
void showGameNotification();
}
