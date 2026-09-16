#pragma once
#include <string>
#include <type_traits>
#include <cstdint>
#include <cassert>
#include <vector>
#include <utility>
constexpr int SSD1306_WHITE = 1;
class Adafruit_SSD1306 {
public:
  std::string text;
  struct TextRun { int x, y; std::string value; };
  std::vector<TextRun> textRuns;
  unsigned draws = 0;
  unsigned bitmaps = 0, pixels = 0;
  std::vector<std::pair<int,int>> framePixels;
  int cursorX = 0, cursorY = 0, textSize = 1;
  void clearDisplay() { text.clear(); framePixels.clear(); textRuns.clear(); }
  void setTextColor(int) {}
  void setTextSize(int size) { textSize = size; }
  void setCursor(int x, int y) { cursorX = x; cursorY = y; text += '\n'; }
  void drawLine(int x1, int y1, int x2, int y2, int) {
    assert(x1 >= 0 && x1 < 128 && x2 >= 0 && x2 < 128);
    assert(y1 >= 0 && y1 < 64 && y2 >= 0 && y2 < 64);
  }
  void drawBitmap(int x, int y, const uint8_t*, int width, int height, int) {
    assert(x >= 0 && y >= 0 && x + width <= 128 && y + height <= 64); ++bitmaps;
  }
  void drawPixel(int x, int y, int) {
    assert(x >= 0 && y >= 0 && x < 128 && y < 64); ++pixels;
    framePixels.emplace_back(x,y);
  }
  void print(const char* value) {
    textRuns.push_back({cursorX, cursorY, value});
    for (const char* p = value; *p; ++p) {
      assert(cursorX >= 0 && cursorX + 6 * textSize <= 128);
      assert(cursorY >= 0 && cursorY + 8 * textSize <= 64);
      cursorX += 6 * textSize;
    }
    text += value;
  }
  template<class T, typename = std::enable_if_t<std::is_integral<T>::value>>
  void print(T value) { print(std::to_string(value).c_str()); }
  void println(const char* value) { text += value; text += '\n'; }
  void display() { ++draws; }
};
