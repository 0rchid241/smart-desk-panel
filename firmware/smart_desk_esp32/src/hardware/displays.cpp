#include "displays.h"
#include "../core/app_config.h"
#include <Wire.h>
using namespace AppConfig;
namespace Displays {
namespace {
// OLED 1: Smart Desk / 추후 GAME 그래픽 화면
// SDA = GPIO21, SCL = GPIO22
Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);


// OLED 2: 평상시 Desk Pet / 추후 GAME 텍스트 화면
// SDA = GPIO16, SCL = GPIO17
TwoWire gameWire =
  TwoWire(1);

Adafruit_SSD1306 gameDisplay(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &gameWire,
  OLED_RESET
);




} // namespace

void init() {
  Wire.begin(
    DESK_SDA_PIN,
    DESK_SCL_PIN
  );

  gameWire.begin(
    GAME_SDA_PIN,
    GAME_SCL_PIN
  );


  // -------------------------
  // OLED 1: Smart Desk
  // -------------------------

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      SCREEN_ADDRESS,
      true,
      false
    )
  ) {
    Serial.println(
      "Desk OLED init failed"
    );

    while (true) {
      delay(1000);
    }
  }


  // -------------------------
  // OLED 2: Desk Pet / Game
  // -------------------------

  if (
    !gameDisplay.begin(
      SSD1306_SWITCHCAPVCC,
      SCREEN_ADDRESS,
      true,
      false
    )
  ) {
    Serial.println(
      "Game OLED init failed"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println(
    "Both OLEDs initialized"
  );


}

Adafruit_SSD1306& desk() { return display; }

Adafruit_SSD1306& game() { return gameDisplay; }
} // namespace Displays
