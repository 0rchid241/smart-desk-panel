#include "app.h"

#include <Arduino.h>

#include "../services/network_service.h"

namespace App {

void init() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("DeskMon v2 boot");

  NetworkService::init();
}

void update() {
  NetworkService::update();
}

}  // namespace App
