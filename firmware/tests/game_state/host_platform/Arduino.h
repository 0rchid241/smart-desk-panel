#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
using String = std::string;
inline unsigned long hostMillis = 1234;
inline unsigned long millis() { return hostMillis; }
inline uint8_t pgm_read_byte(const uint8_t* p) { return *p; }
struct HostSerial {
  template<class T> void print(const T&) {}
  template<class T> void println(const T&) {}
};
inline HostSerial Serial;
