#pragma once
#include <string>
#include <type_traits>
#include <cstdint>
constexpr int SSD1306_WHITE = 1;
class Adafruit_SSD1306 {
public:
  std::string text;
  unsigned draws = 0;
  void clearDisplay() { text.clear(); }
  void setTextColor(int) {}
  void setTextSize(int) {}
  void setCursor(int, int) { text += '\n'; }
  void drawLine(int, int, int, int, int) {}
  void print(const char* value) { text += value; }
  template<class T, typename = std::enable_if_t<std::is_integral<T>::value>>
  void print(T value) { text += std::to_string(value); }
  void println(const char* value) { text += value; text += '\n'; }
  void display() { ++draws; }
};
