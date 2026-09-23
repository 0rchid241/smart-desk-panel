#include "app.h"

#include <Arduino.h>

#include "../services/display_service.h"
#include "../services/lin_server_service.h"
#include "../services/network_service.h"

namespace App {

void init() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("DeskMon v2 boot");

  DisplayService::init();
  NetworkService::init();
  LinServerService::init();
}

void update() {
  NetworkService::update();
  LinServerService::update();
  DisplayService::update();
}

}  // namespace App
