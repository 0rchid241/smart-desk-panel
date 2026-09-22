#pragma once
// Host-only NVS fake. Never part of the Arduino sketch or production includes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <limits>

namespace FakeNvs {
inline std::map<std::string, std::vector<uint8_t>> data;
inline bool failOpen = false, failRead = false, partialWrite = false, corruptWrite = false;
inline bool failReadAfterWrite = false, rejectWrite = false;
inline unsigned writes = 0;
inline size_t readBudget = std::numeric_limits<size_t>::max();
inline void reset() {
  data.clear(); failOpen = failRead = partialWrite = corruptWrite = false; writes = 0;
  failReadAfterWrite = rejectWrite = false;
  readBudget = std::numeric_limits<size_t>::max();
}
}
class Preferences {
  std::string space;
public:
  bool begin(const char* name, bool) { space = name; return !FakeNvs::failOpen; }
  void end() {}
  bool isKey(const char* key) { return FakeNvs::data.count(space + "/" + key) != 0; }
  size_t getBytesLength(const char* key) { return FakeNvs::data.at(space + "/" + key).size(); }
  size_t getBytes(const char* key, void* output, size_t capacity) {
    if (FakeNvs::failRead) return 0;
    const auto& bytes = FakeNvs::data.at(space + "/" + key);
    if (capacity < bytes.size()) return 0;
    std::memcpy(output, bytes.data(), bytes.size());
    return bytes.size();
  }
  size_t putBytes(const char* key, const void* input, size_t length) {
    ++FakeNvs::writes;
    if (FakeNvs::rejectWrite) return 0;
    const auto* bytes = static_cast<const uint8_t*>(input);
    const size_t written = FakeNvs::partialWrite ? length / 2 : length;
    auto& stored = FakeNvs::data[space + "/" + key];
    stored.assign(bytes, bytes + written);
    if (FakeNvs::corruptWrite && !stored.empty()) stored.back() ^= 1;
    if (FakeNvs::failReadAfterWrite) FakeNvs::failRead = true;
    return written;
  }
};
