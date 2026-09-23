#include "app.h"

#include <Arduino.h>

#include "../services/display_service.h"
#include "../services/input_service.h"
#include "../services/lin_server_service.h"
#include "../services/mode_service.h"
#include "../services/network_service.h"
#include "../services/time_service.h"

namespace App {

void init() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("DeskMon v2 boot");

  DisplayService::init();
  InputService::init();
  ModeService::init();
  NetworkService::init();
  TimeService::init();
  LinServerService::init();
}

void update() {
  NetworkService::update();
  TimeService::update();
  LinServerService::update();

  InputService::update();

  const InputService::Event event =
      InputService::consumeEvent();

  switch (event) {
    case InputService::Event::MODE_SWITCH:
      ModeService::toggle();
      break;

    case InputService::Event::LEFT:
      Serial.println("Button: LEFT");
      break;

    case InputService::Event::OK:
      Serial.println("Button: OK");
      break;

    case InputService::Event::RIGHT:
      Serial.println("Button: RIGHT");
      break;

    case InputService::Event::BACK:
      Serial.println("Button: BACK");
      break;

    case InputService::Event::NONE:
      break;
  }

  DisplayService::update();
}

}  // namespace App
