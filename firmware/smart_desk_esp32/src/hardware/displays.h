#pragma once
#include "../core/app_types.h"
#include <Adafruit_SSD1306.h>

namespace Displays {
// Owns both OLED objects and the second I2C bus for the application lifetime.
void init();
Adafruit_SSD1306& desk();
Adafruit_SSD1306& game();
}
