#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

void drawUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  const char* text
);

void drawUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  const String& text
);