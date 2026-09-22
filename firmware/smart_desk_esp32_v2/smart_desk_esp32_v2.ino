#include "src/core/app.h"

void setup() {
  App::init();
}

void loop() {
  App::update();
  delay(20);
}
