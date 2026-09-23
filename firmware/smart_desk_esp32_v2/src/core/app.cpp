#include "app.h"

#include <Arduino.h>

#include "../services/display_service.h"
#include "../services/input_service.h"
#include "../services/lin_server_service.h"
#include "../services/network_service.h"

namespace App {

void init() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("DeskMon v2 boot");

  DisplayService::init();
  InputService::init();
  NetworkService::init();
  LinServerService::init();
}

void update() {
  NetworkService::update();
  LinServerService::update();
  InputService::update();
  DisplayService::update();

  if (InputService::consumePressed(
          InputService::Button::LEFT)) {
    Serial.println("Button: LEFT");
  }

  if (InputService::consumePressed(
          InputService::Button::OK)) {
    Serial.println("Button: OK");
  }

  if (InputService::consumePressed(
          InputService::Button::RIGHT)) {
    Serial.println("Button: RIGHT");
  }
}

}  // namespace App
