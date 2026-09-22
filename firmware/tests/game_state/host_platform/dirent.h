#pragma once
// Host-only POSIX boundary for LittleFS VFS directory enumeration. EOF leaves
// errno unchanged; failures return nullptr with errno set, just like readdir.
#include "LittleFS.h"
#include <cerrno>
constexpr unsigned char DT_UNKNOWN = 0, DT_DIR = 4, DT_REG = 8;
struct dirent { unsigned char d_type = DT_UNKNOWN; char d_name[256] = {}; };
struct DIR {
  std::vector<std::pair<std::string, unsigned char>> entries;
  size_t position = 0;
  dirent entry;
};
inline DIR* opendir(const char* path) {
  if (!FakeLittleFS::mounted || FakeLittleFS::failDirOpen || FakeLittleFS::failOpen) { errno = EIO; return nullptr; }
  if (std::strcmp(path, "/littlefs/pokemon") != 0) { errno = ENOENT; return nullptr; }
  if (!FakeLittleFS::directories.count("/pokemon")) { errno = ENOENT; return nullptr; }
  auto* directory = new DIR;
  const std::string prefix = "/pokemon/";
  const auto append = [&](const std::string& name, unsigned char type) {
    if (name.compare(0, prefix.size(), prefix) != 0) return;
    const std::string base = name.substr(prefix.size());
    if (base.empty() || base.find('/') != std::string::npos) return;
    directory->entries.emplace_back(base, type);
  };
  for (const auto& file : FakeLittleFS::files) append(file.first, DT_REG);
  for (const auto& name : FakeLittleFS::directories) append(name, DT_DIR);
  ++FakeLittleFS::directoryOpens;
  return directory;
}
inline dirent* readdir(DIR* directory) {
  if (!FakeLittleFS::mounted || !FakeLittleFS::directoryReadBudget) { errno = EIO; return nullptr; }
  if (directory->position == directory->entries.size()) return nullptr;
  --FakeLittleFS::directoryReadBudget;
  const auto& entry = directory->entries[directory->position++];
  if (entry.first.size() >= sizeof(directory->entry.d_name)) { errno = EIO; return nullptr; }
  std::memcpy(directory->entry.d_name, entry.first.c_str(), entry.first.size() + 1);
  directory->entry.d_type = entry.second;
  return &directory->entry;
}
inline int closedir(DIR* directory) {
  delete directory; ++FakeLittleFS::directoryCloses;
  if (FakeLittleFS::failDirClose) { errno = EIO; return -1; }
  return 0;
}
