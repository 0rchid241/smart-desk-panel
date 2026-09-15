#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
using String = std::string;
inline unsigned long millis() { return 1234; }
struct HostSerial {
  template<class T> void print(const T&) {}
  template<class T> void println(const T&) {}
};
inline HostSerial Serial;
