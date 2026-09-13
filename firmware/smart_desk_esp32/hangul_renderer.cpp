#include "hangul_renderer.h"

#include "hangul_font_16x16.h"


// 한 줄을 임시로 그리는 128x16 흑백 버퍼.
// 이 안에서 잘라낸 뒤 OLED에 복사한다.
static GFXcanvas1 lineCanvas(
  128,
  16
);


// --------------------------------------------------
// UTF-8 문자 하나 읽기
// --------------------------------------------------

static uint32_t readUtf8Char(
  const char*& text
) {
  uint8_t first =
    static_cast<uint8_t>(
      *text++
    );


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
      static_cast<uint8_t>(
        *text++
      );

    return
      ((first & 0x1F) << 6) |
      (second & 0x3F);
  }


  // 3 byte UTF-8
  // 한글 완성형
  if (
    (first & 0xF0) ==
    0xE0
  ) {
    uint8_t second =
      static_cast<uint8_t>(
        *text++
      );

    uint8_t third =
      static_cast<uint8_t>(
        *text++
      );

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
      static_cast<uint8_t>(
        *text++
      );

    uint8_t third =
      static_cast<uint8_t>(
        *text++
      );

    uint8_t fourth =
      static_cast<uint8_t>(
        *text++
      );

    return
      ((first & 0x07) << 18) |
      ((second & 0x3F) << 12) |
      ((third & 0x3F) << 6) |
      (fourth & 0x3F);
  }


  return '?';
}


// --------------------------------------------------
// 문자 하나의 폭
// --------------------------------------------------

static int16_t getCodepointWidth(
  uint32_t codepoint
) {
  if (
    codepoint >=
      HANGUL_FONT_START &&
    codepoint <=
      HANGUL_FONT_END
  ) {
    return
      HANGUL_FONT_WIDTH;
  }


  // ASCII 및 지원하지 않는 문자는
  // 기본 6px 폭으로 처리
  return 6;
}


// --------------------------------------------------
// 한글 한 글자 출력
// Adafruit_GFX 계열 어디든 출력 가능
// --------------------------------------------------

static void drawHangulChar(
  Adafruit_GFX& target,
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


  target.drawBitmap(
    x,
    y,
    hangul_font_16x16[index],
    HANGUL_FONT_WIDTH,
    HANGUL_FONT_HEIGHT,
    1
  );
}


// --------------------------------------------------
// 기존 UTF-8 문자열 출력
// 자동 줄바꿈 지원
// --------------------------------------------------

void drawUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  const char* text
) {
  int16_t x =
    startX;

  int16_t y =
    startY;


  while (
    *text != '\0'
  ) {

    uint32_t codepoint =
      readUtf8Char(
        text
      );


    if (
      codepoint == '\n'
    ) {
      x =
        startX;

      y += 18;

      continue;
    }


    int16_t charWidth =
      getCodepointWidth(
        codepoint
      );


    if (
      x + charWidth >
      display.width()
    ) {
      x =
        startX;

      y += 18;
    }


    // 한글
    if (
      codepoint >=
        HANGUL_FONT_START &&
      codepoint <=
        HANGUL_FONT_END
    ) {

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
    if (
      codepoint == ' '
    ) {
      x += 6;

      continue;
    }


    // ASCII
    if (
      codepoint < 128
    ) {

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

      continue;
    }


    // 현재 지원하지 않는 Unicode
    display.setTextSize(1);

    display.setTextColor(
      SSD1306_WHITE
    );

    display.setCursor(
      x,
      y + 4
    );

    display.write('?');

    x += 6;
  }
}


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


// --------------------------------------------------
// UTF-8 문자열 전체 폭 계산
// --------------------------------------------------

int16_t measureUtf8TextWidth(
  const char* text
) {
  int16_t width =
    0;


  while (
    *text != '\0'
  ) {

    uint32_t codepoint =
      readUtf8Char(
        text
      );


    if (
      codepoint == '\n'
    ) {
      break;
    }


    width +=
      getCodepointWidth(
        codepoint
      );
  }


  return width;
}


int16_t measureUtf8TextWidth(
  const String& text
) {
  return
    measureUtf8TextWidth(
      text.c_str()
    );
}


// --------------------------------------------------
// 한 줄 클리핑 출력
// --------------------------------------------------

void drawUtf8TextLineClipped(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  int16_t viewportWidth,
  const char* text,
  int16_t scrollOffset
) {
  if (
    viewportWidth <= 0
  ) {
    return;
  }


  if (
    viewportWidth > 128
  ) {
    viewportWidth = 128;
  }


  // 임시 라인 버퍼 초기화
  lineCanvas.fillScreen(0);

  lineCanvas.setTextColor(1);

  lineCanvas.setTextSize(1);

  lineCanvas.setTextWrap(false);


  // scrollOffset만큼 왼쪽으로 이동
  int16_t x =
    -scrollOffset;


  while (
    *text != '\0'
  ) {

    uint32_t codepoint =
      readUtf8Char(
        text
      );


    // 여기서는 한 줄 전용
    if (
      codepoint == '\n'
    ) {
      break;
    }


    int16_t charWidth =
      getCodepointWidth(
        codepoint
      );


    // 화면에 걸쳐 있는 문자만 실제 렌더링
    bool visible =
      (
        x <
        viewportWidth
      ) &&
      (
        x + charWidth >
        0
      );


    if (visible) {

      // 한글
      if (
        codepoint >=
          HANGUL_FONT_START &&
        codepoint <=
          HANGUL_FONT_END
      ) {

        drawHangulChar(
          lineCanvas,
          x,
          0,
          codepoint
        );

      }

      // 공백
      else if (
        codepoint == ' '
      ) {
        // 그릴 필요 없음
      }

      // ASCII
      else if (
        codepoint < 128
      ) {

        lineCanvas.setCursor(
          x,
          4
        );

        lineCanvas.write(
          static_cast<char>(
            codepoint
          )
        );

      }

      // 미지원 Unicode
      else {

        lineCanvas.setCursor(
          x,
          4
        );

        lineCanvas.write('?');
      }
    }


    x +=
      charWidth;


    // 나머지 글자가 전부 오른쪽 바깥이면
    // 계속 진행할 필요는 있지만,
    // 이미 모든 텍스트 폭 계산은 끝난 상태가 아니므로
    // 여기서는 그대로 순회한다.
  }


  // 지정된 viewport 영역만 OLED로 복사
  for (
    int16_t canvasY = 0;
    canvasY < 16;
    canvasY++
  ) {

    for (
      int16_t canvasX = 0;
      canvasX < viewportWidth;
      canvasX++
    ) {

      if (
        lineCanvas.getPixel(
          canvasX,
          canvasY
        )
      ) {

        display.drawPixel(
          startX +
            canvasX,
          startY +
            canvasY,
          SSD1306_WHITE
        );
      }
    }
  }
}


void drawUtf8TextLineClipped(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  int16_t viewportWidth,
  const String& text,
  int16_t scrollOffset
) {
  drawUtf8TextLineClipped(
    display,
    startX,
    startY,
    viewportWidth,
    text.c_str(),
    scrollOffset
  );
}


// --------------------------------------------------
// 자동 스크롤 위치 계산
// --------------------------------------------------

static int16_t calculateScrollOffset(
  int16_t textWidth,
  int16_t viewportWidth
) {
  if (
    textWidth <=
    viewportWidth
  ) {
    return 0;
  }


  const unsigned long START_PAUSE_MS =
    1200;

  const unsigned long PIXEL_STEP_MS =
    70;

  const unsigned long END_PAUSE_MS =
    1000;


  int16_t travel =
    textWidth -
    viewportWidth;


  unsigned long moveDuration =
    static_cast<unsigned long>(
      travel
    ) *
    PIXEL_STEP_MS;


  unsigned long cycleDuration =
    START_PAUSE_MS +
    moveDuration +
    END_PAUSE_MS;


  unsigned long position =
    millis() %
    cycleDuration;


  // 시작 위치에서 잠시 정지
  if (
    position <
    START_PAUSE_MS
  ) {
    return 0;
  }


  position -=
    START_PAUSE_MS;


  // 왼쪽으로 스크롤
  if (
    position <
    moveDuration
  ) {

    int16_t offset =
      position /
      PIXEL_STEP_MS;


    if (
      offset >
      travel
    ) {
      offset =
        travel;
    }


    return offset;
  }


  // 마지막 부분에서 잠시 정지
  return travel;
}


// --------------------------------------------------
// 긴 문자열 자동 전광판 출력
// --------------------------------------------------

void drawScrollingUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  int16_t viewportWidth,
  const char* text
) {

  int16_t textWidth =
    measureUtf8TextWidth(
      text
    );


  int16_t scrollOffset =
    calculateScrollOffset(
      textWidth,
      viewportWidth
    );


  drawUtf8TextLineClipped(
    display,
    startX,
    startY,
    viewportWidth,
    text,
    scrollOffset
  );
}


void drawScrollingUtf8Text(
  Adafruit_SSD1306& display,
  int16_t startX,
  int16_t startY,
  int16_t viewportWidth,
  const String& text
) {
  drawScrollingUtf8Text(
    display,
    startX,
    startY,
    viewportWidth,
    text.c_str()
  );
}