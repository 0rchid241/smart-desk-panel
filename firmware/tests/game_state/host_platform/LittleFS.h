#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <functional>

namespace FakeLittleFS {
using Bytes = std::vector<uint8_t>;
inline std::map<std::string, std::shared_ptr<Bytes>> files;
inline std::set<std::string> directories;
inline bool mounted = false, failMount = false, formatRequested = false;
inline bool failFormat = false, needsFormat = false;
inline unsigned formats = 0;
inline bool failDirOpen = false, failDirClose = false;
inline size_t directoryReadBudget = std::numeric_limits<size_t>::max();
inline size_t removeBudget = std::numeric_limits<size_t>::max();
inline size_t directoryOpens = 0, directoryCloses = 0;
inline std::vector<std::string> removeAttempts;
inline std::function<void(const char*)> beforeRemove;
inline size_t readCalls = 0, readBytes = 0, writeCalls = 0, readOpens = 0, readCloses = 0;
inline bool failOpen = false, failMkdir = false, failRemove = false, failSeek = false;
inline bool failReopen = false, corruptFlush = false, truncateClose = false;
inline size_t writeBudget = std::numeric_limits<size_t>::max();
inline size_t readBudget = std::numeric_limits<size_t>::max();
inline void reset() {
  readCalls = readBytes = writeCalls = readOpens = readCloses = 0;
  failDirOpen = failDirClose = false;
  directoryReadBudget = removeBudget = std::numeric_limits<size_t>::max();
  directoryOpens = directoryCloses = 0; removeAttempts.clear(); beforeRemove = {};
  files.clear(); directories.clear(); mounted = failMount = formatRequested = false;
  failFormat = needsFormat = false; formats = 0;
  failOpen = failMkdir = failRemove = failSeek = failReopen = corruptFlush = truncateClose = false;
  writeBudget = readBudget = std::numeric_limits<size_t>::max();
}
}
namespace fs {
class File {
public:
  File() = default;
  File(std::shared_ptr<FakeLittleFS::Bytes> bytes, bool write) : bytes_(bytes), write_(write) {}
  explicit operator bool() const { return bytes_ && FakeLittleFS::mounted; }
  size_t size() const { return bytes_ ? bytes_->size() : 0; }
  size_t read(uint8_t* output, size_t length) {
    ++FakeLittleFS::readCalls;
    if (!*this || position_ > bytes_->size()) return 0;
    size_t count = std::min(length, bytes_->size() - position_);
    count = std::min(count, FakeLittleFS::readBudget);
    std::memcpy(output, bytes_->data() + position_, count);
    position_ += count; FakeLittleFS::readBudget -= count; FakeLittleFS::readBytes += count;
    return count;
  }
  size_t write(const uint8_t* input, size_t length) {
    ++FakeLittleFS::writeCalls;
    if (!*this || !write_) return 0;
    const size_t count = std::min(length, FakeLittleFS::writeBudget);
    if (position_ + count > bytes_->size()) bytes_->resize(position_ + count);
    std::memcpy(bytes_->data() + position_, input, count);
    position_ += count; FakeLittleFS::writeBudget -= count;
    return count;
  }
  bool seek(uint32_t position) {
    if (!*this || FakeLittleFS::failSeek) return false;
    position_ = position; return true;
  }
  void flush() {
    if (*this && write_ && FakeLittleFS::corruptFlush && !bytes_->empty()) {
      bytes_->back() ^= 1; FakeLittleFS::corruptFlush = false;
    }
  }
  void close() {
    if (bytes_ && !write_) ++FakeLittleFS::readCloses;
    if (bytes_ && write_) {
      if (FakeLittleFS::truncateClose && !bytes_->empty()) {
        bytes_->pop_back(); FakeLittleFS::truncateClose = false;
      }
      if (FakeLittleFS::failReopen) {
        FakeLittleFS::failOpen = true; FakeLittleFS::failReopen = false;
      }
    }
    bytes_.reset(); position_ = 0;
  }
private:
  std::shared_ptr<FakeLittleFS::Bytes> bytes_;
  size_t position_ = 0;
  bool write_ = false;
};
}
class HostLittleFS {
public:
  bool begin(bool format = false) {
    FakeLittleFS::formatRequested = format;
    FakeLittleFS::mounted = !FakeLittleFS::failMount && !FakeLittleFS::needsFormat;
    return FakeLittleFS::mounted;
  }
  void end() { FakeLittleFS::mounted = false; }
  bool format() {
    ++FakeLittleFS::formats;
    if (FakeLittleFS::failFormat) return false;
    FakeLittleFS::files.clear(); FakeLittleFS::directories.clear();
    FakeLittleFS::needsFormat = false;
    return true;
  }
  bool exists(const char* path) {
    return FakeLittleFS::mounted &&
      (FakeLittleFS::files.count(path) != 0 || FakeLittleFS::directories.count(path) != 0);
  }
  bool mkdir(const char* path) {
    if (!FakeLittleFS::mounted || FakeLittleFS::failMkdir) return false;
    FakeLittleFS::directories.insert(path); return true;
  }
  bool remove(const char* path) {
    FakeLittleFS::removeAttempts.emplace_back(path);
    if (FakeLittleFS::beforeRemove) FakeLittleFS::beforeRemove(path);
    if (!FakeLittleFS::mounted || FakeLittleFS::failRemove || !FakeLittleFS::removeBudget) return false;
    if (!FakeLittleFS::files.erase(path)) return false;
    --FakeLittleFS::removeBudget;
    return true;
  }
  fs::File open(const char* path, const char* mode) {
    if (!FakeLittleFS::mounted || FakeLittleFS::failOpen) return {};
    const bool writing = std::strcmp(mode, "w") == 0;
    if (writing) FakeLittleFS::files[path] = std::make_shared<FakeLittleFS::Bytes>();
    auto found = FakeLittleFS::files.find(path);
    if (!writing && found != FakeLittleFS::files.end()) ++FakeLittleFS::readOpens;
    return found == FakeLittleFS::files.end() ? fs::File{} : fs::File(found->second, writing);
  }
};
inline HostLittleFS LittleFS;
