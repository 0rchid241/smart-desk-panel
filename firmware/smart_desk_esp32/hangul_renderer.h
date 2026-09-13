#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// 기존 일반 UTF-8 출력
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


// UTF-8 문자열의 픽셀 너비 계산
int16_t measureUtf8TextWidth(
  const char* text
);

int16_t measureUtf8TextWidth(
  const String& text
);


// 한 줄 영역 안에서만 출력.
// 줄바꿈하지 않고 영역 밖은 잘라낸다.
void drawUtf8TextLineClipped(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  int16_t viewportWidth,
  const char* text,
  int16_t scrollOffset = 0
);

void drawUtf8TextLineClipped(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  int16_t viewportWidth,
  const String& text,
  int16_t scrollOffset = 0
);


// 긴 문자열만 자동으로 좌우 스크롤
void drawScrollingUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  int16_t viewportWidth,
  const char* text
);

void drawScrollingUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  int16_t viewportWidth,
  const String& text
);