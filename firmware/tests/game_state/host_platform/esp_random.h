#pragma once
#include <cstdint>
#include <vector>
namespace FakeRandom {
inline uint32_t next = 1;
inline std::vector<uint32_t> values;
inline size_t position = 0;
inline void reset() { next = 1; values.clear(); position = 0; }
}
inline uint32_t esp_random() {
  if (FakeRandom::position < FakeRandom::values.size()) return FakeRandom::values[FakeRandom::position++];
  return FakeRandom::next++;
}
