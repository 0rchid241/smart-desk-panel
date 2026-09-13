#include "hangul_renderer.h"

#include "hangul_font_16x16.h"


// --------------------------------------------------
// UTF-8 문자 하나 읽기
// --------------------------------------------------

static uint32_t readUtf8Char(
  const char*& text
) {
  uint8_t first =
    static_cast<uint8_t>(*text++);

  // ASCII
  if (first < 0x80) {
    return first;
  }

  // 2 byte UTF-8
  if (
    (first & 0xE0) ==
    0xC0
  ) {
    uint8_t second =
      static_cast<uint8_t>(*text++);

    return
      ((first & 0x1F) << 6) |
      (second & 0x3F);
  }

  // 3 byte UTF-8
  // 한글 완성형은 여기에 해당
  if (
    (first & 0xF0) ==
    0xE0
  ) {
    uint8_t second =
      static_cast<uint8_t>(*text++);

    uint8_t third =
      static_cast<uint8_t>(*text++);

    return
      ((first & 0x0F) << 12) |
      ((second & 0x3F) << 6) |
      (third & 0x3F);
  }

  // 4 byte UTF-8
  if (
    (first & 0xF8) ==
    0xF0
  ) {
    uint8_t second =
      static_cast<uint8_t>(*text++);

    uint8_t third =
      static_cast<uint8_t>(*text++);

    uint8_t fourth =
      static_cast<uint8_t>(*text++);

    return
      ((first & 0x07) << 18) |
      ((second & 0x3F) << 12) |
      ((third & 0x3F) << 6) |
      (fourth & 0x3F);
  }

  return '?';
}


// --------------------------------------------------
// 한글 한 글자
// --------------------------------------------------

static void drawHangulChar(
  Adafruit_SSD1306& display,
  int16_t x,
  int16_t y,
  uint32_t codepoint
) {
  if (
    codepoint <
      HANGUL_FONT_START ||
    codepoint >
      HANGUL_FONT_END
  ) {
    return;
  }

  uint32_t index =
    codepoint -
    HANGUL_FONT_START;

  display.drawBitmap(
    x,
    y,
    hangul_font_16x16[index],
    HANGUL_FONT_WIDTH,
    HANGUL_FONT_HEIGHT,
    SSD1306_WHITE
  );
}


// --------------------------------------------------
// UTF-8 문자열
// --------------------------------------------------

void drawUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  const char* text
) {
  int16_t x = startX;
  int16_t y = startY;

  while (*text != '\0') {

    uint32_t codepoint =
      readUtf8Char(text);

    // 줄바꿈
    if (codepoint == '\n') {
      x = startX;
      y += 18;
      continue;
    }

    // 현대 한글 완성형
    if (
      codepoint >=
        HANGUL_FONT_START &&
      codepoint <=
        HANGUL_FONT_END
    ) {

      // 화면 오른쪽을 넘으면 다음 줄
      if (
        x +
        HANGUL_FONT_WIDTH >
        display.width()
      ) {
        x = startX;
        y += 18;
      }

      drawHangulChar(
        display,
        x,
        y,
        codepoint
      );

      x +=
        HANGUL_FONT_WIDTH;

      continue;
    }

    // 공백
    if (codepoint == ' ') {
      x += 6;
      continue;
    }

    // 기본 ASCII
    if (codepoint < 128) {

      if (
        x + 6 >
        display.width()
      ) {
        x = startX;
        y += 18;
      }

      display.setTextSize(1);

      display.setTextColor(
        SSD1306_WHITE
      );

      display.setCursor(
        x,
        y + 4
      );

      display.write(
        static_cast<char>(
          codepoint
        )
      );

      x += 6;
    }
  }
}


// --------------------------------------------------
// Arduino String 지원
// --------------------------------------------------

void drawUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  const String& text
) {
  drawUtf8Text(
    display,
    startX,
    startY,
    text.c_str()
  );
}