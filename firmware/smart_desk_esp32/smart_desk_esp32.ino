#include "src/core/app_types.h"
#include "src/hardware/displays.h"
#include "src/hardware/buttons.h"
#include "src/services/network_time.h"
#include "src/services/calendar_service.h"
#include "src/desk/desk_app.h"
#include "src/game/game_app.h"

static DeviceMode deviceMode = MODE_DESK;

static void switchDeviceMode() {
  if (deviceMode == MODE_DESK) {
    deviceMode = MODE_GAME;
    Serial.println("Mode changed: GAME");
    GameApp::drawGameGraphics();
    if (DeskApp::notificationActive()) {
      DeskApp::showGameNotification();
    } else {
      GameApp::drawGameTextScreen();
    }
  } else {
    deviceMode = MODE_DESK;
    Serial.println("Mode changed: DESK");
    GameApp::drawDeskPet();
  }
}

static void handleButton(ButtonEvent button) {
  if (button == BUTTON_NONE) return;
  if (button == BUTTON_MODE_SWITCH) {
    switchDeviceMode();
    return;
  }
  // Notifications take priority in both modes, after the mode chord.
  if (DeskApp::notificationActive()) {
    if (button == BUTTON_OK) {
      DeskApp::dismissNotification();
      if (deviceMode == MODE_GAME) GameApp::drawGameTextScreen();
    }
    return;
  }
  if (deviceMode == MODE_GAME) {
    GameApp::handleButton(button);
  } else {
    DeskApp::handleButton(button);
  }
}

void setup() {
  Serial.begin(115200);
  Buttons::init();
  Displays::init();
  DeskApp::showMessage("BOOTING...");
  GameApp::init();
  delay(500);

  // Restore cache and reminder flags before attempting a network connection.
  CalendarService::init();
  if (NetworkTime::init(DeskApp::showMessage)) {
    CalendarService::syncCalendarEvents();
  }
  CalendarService::startSyncClock();
  NetworkTime::startReconnectClock();
  DeskApp::init();
  deviceMode = MODE_DESK;
  Serial.println("Mode: DESK");
}

void loop() {
  handleButton(Buttons::readButtonEvent());
  const bool reconnected = NetworkTime::update();
  CalendarService::update(reconnected);
  CalendarService::checkScheduleReminders(
    DeskApp::notificationActive(), DeskApp::triggerNotification);
  DeskApp::update();

  if (deviceMode == MODE_DESK) {
    DeskApp::render();
  } else if (DeskApp::notificationActive()) {
    DeskApp::showGameNotification();
  } else {
    GameApp::drawGameTextScreen();
  }
  GameApp::updatePokemonAnimation(deviceMode);
  delay(20);
}
