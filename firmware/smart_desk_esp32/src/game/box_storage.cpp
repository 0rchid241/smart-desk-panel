#include "box_storage.h"
#include "crc32.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <cerrno>

namespace BoxStorage {
using namespace PokemonGame;
namespace {
bool mounted = false;
bool sameKey(BoxKey a, BoxKey b) {
  return a.storeId == b.storeId && a.generation == b.generation;
}
bool readRecord(fs::File& file, uint32_t slot, uint8_t* bytes) {
  return file.seek(static_cast<uint32_t>(BOX_RECORDS_OFFSET + slot * POKEMON_RECORD_SIZE)) &&
         file.read(bytes, POKEMON_RECORD_SIZE) == POKEMON_RECORD_SIZE;
}
uint32_t headerCrc(const uint8_t* bytes) {
  uint32_t crc = updateCrc32(0xffffffffu, bytes, BOX_CRC_OFFSET);
  return updateCrc32(crc, bytes + BOX_CRC_OFFSET + 4, BOX_HEADER_SIZE - BOX_CRC_OFFSET - 4);
}
}
bool snapshotPath(BoxKey key, char* output, size_t capacity) {
  if (!output || !capacity || !key.storeId || !key.generation) return false;
  const int length = std::snprintf(output, capacity, "/pokemon/box_%016llx_%016llx.bin",
    static_cast<unsigned long long>(key.storeId), static_cast<unsigned long long>(key.generation));
  return length > 0 && static_cast<size_t>(length) < capacity;
}
bool parseSnapshotName(const char* name, BoxKey& output) {
  if (!name || std::strlen(name) != 41 || std::strncmp(name, "box_", 4) != 0 ||
      name[20] != '_' || std::strcmp(name + 37, ".bin") != 0) return false;
  BoxKey key;
  uint64_t* fields[] = {&key.storeId, &key.generation};
  for (unsigned field = 0; field < 2; ++field) {
    for (unsigned digit = 0; digit < 16; ++digit) {
      const char c = name[4 + field * 17 + digit];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
      *fields[field] = (*fields[field] << 4) | static_cast<uint64_t>(c <= '9' ? c - '0' : c - 'a' + 10);
    }
  }
  if (!key.storeId || !key.generation) return false;
  output = key;
  return true;
}
Result visitSnapshotKeys(SnapshotVisitor visitor, void* context) {
  if (!mounted) return Result::NotMounted;
  if (!visitor) return Result::InvalidArgument;
  // /littlefs is the unchanged Arduino LittleFS.begin() default VFS mountpoint.
  DIR* directory = opendir("/littlefs/pokemon");
  if (!directory) return errno == ENOENT ? Result::Ok : Result::IoError;
  Result result = Result::Ok;
  while (true) {
    errno = 0;
    const auto* entry = readdir(directory);
    if (!entry) { if (errno) result = Result::IoError; break; }
    BoxKey key;
    // Unknown file types are preserved, not guessed to be regular files.
    if (entry->d_type == DT_REG && parseSnapshotName(entry->d_name, key)) visitor(key, context);
  }
  if (closedir(directory) != 0) result = Result::IoError;
  return result;
}
Result mount() {
  if (mounted) return Result::Ok;
  mounted = LittleFS.begin(false);
  return mounted ? Result::Ok : Result::IoError;
}
void unmount() { LittleFS.end(); mounted = false; }
Result initializeFilesystem() {
  unmount();
  if (!LittleFS.format()) return Result::IoError;
  return mount();
}
Result exists(BoxKey key, bool& output) {
  if (!mounted) return Result::NotMounted;
  char path[PATH_SIZE];
  if (!snapshotPath(key, path, sizeof(path))) return Result::InvalidArgument;
  output = LittleFS.exists(path);
  return Result::Ok;
}
Result removeSnapshot(BoxKey key) {
  bool found = false;
  Result result = exists(key, found);
  if (result != Result::Ok) return result;
  if (!found) return Result::Missing;
  char path[PATH_SIZE];
  snapshotPath(key, path, sizeof(path));
  return LittleFS.remove(path) ? Result::Ok : Result::IoError;
}
void Snapshot::close() {
  file_.close(); ready_ = false; metadata_ = {};
  std::memset(bitmap_, 0, sizeof(bitmap_));
}
Result Snapshot::open(BoxKey key) {
  close();
  bool found = false;
  Result result = exists(key, found);
  if (result != Result::Ok) return result;
  if (!found) return Result::Missing;
  char path[PATH_SIZE];
  snapshotPath(key, path, sizeof(path));
  file_ = LittleFS.open(path, "r");
  if (!file_) return Result::IoError;
  result = validateFile(key);
  if (result != Result::Ok) { close(); return result; }
  ready_ = true;
  return Result::Ok;
}
Result Snapshot::validateFile(BoxKey key) {
  uint8_t header[BOX_HEADER_SIZE];
  if (file_.size() != BOX_FILE_SIZE) return Result::Invalid;
  if (file_.read(header, sizeof(header)) != sizeof(header)) return Result::IoError;
  if (!decodeBoxHeader(header, metadata_) || !sameKey(key, metadata_.key)) return Result::Invalid;
  if (file_.read(bitmap_, sizeof(bitmap_)) != sizeof(bitmap_)) return Result::IoError;
  uint32_t crc = updateCrc32(headerCrc(header), bitmap_, sizeof(bitmap_));
  uint32_t count = 0;
  uint8_t bytes[POKEMON_RECORD_SIZE];
  for (uint32_t slot = 0; slot < BOX_CAPACITY; ++slot) {
    if (file_.read(bytes, sizeof(bytes)) != sizeof(bytes)) return Result::IoError;
    crc = updateCrc32(crc, bytes, sizeof(bytes));
    if (boxOccupied(bitmap_, slot)) {
      PokemonInstance pokemon;
      if (!decodePokemonRecord(bytes, pokemon) || !isValidPokemon(pokemon)) return Result::Invalid;
      ++count;
    } else {
      for (uint8_t byte : bytes) if (byte) return Result::Invalid;
    }
  }
  if (count != metadata_.occupiedCount || ~crc != metadata_.crc32) return Result::Invalid;
  return validateUniqueIds();
}
Result Snapshot::validateUniqueIds() {
  // Bounded scratch, not a persistent index: compare groups of 64 IDs against
  // later records. Up to 32 passes at capacity; no 2048-instance RAM allocation.
  uint32_t ids[64];
  uint8_t bytes[POKEMON_RECORD_SIZE];
  for (uint32_t base = 0; base < BOX_CAPACITY; base += 64) {
    size_t count = 0;
    for (uint32_t slot = base; slot < BOX_CAPACITY; ++slot) {
      if (!boxOccupied(bitmap_, slot)) continue;
      if (!readRecord(file_, slot, bytes)) return Result::IoError;
      PokemonInstance pokemon;
      if (!decodePokemonRecord(bytes, pokemon)) return Result::Invalid;
      for (size_t i = 0; i < count; ++i)
        if (ids[i] == pokemon.instanceId) return Result::DuplicateId;
      if (slot < base + 64) ids[count++] = pokemon.instanceId;
      else if (count == 0) break;
    }
  }
  return Result::Ok;
}
Result Snapshot::occupied(uint32_t slot, bool& output) const {
  if (!mounted) return Result::NotMounted;
  if (!ready_) return Result::Invalid;
  if (slot >= BOX_CAPACITY) return Result::InvalidArgument;
  output = boxOccupied(bitmap_, slot);
  return Result::Ok;
}
Result Snapshot::readSlot(uint32_t slot, PokemonInstance& output) {
  bool used = false;
  Result result = occupied(slot, used);
  if (result != Result::Ok) return result;
  if (!used) return Result::EmptySlot;
  uint8_t bytes[POKEMON_RECORD_SIZE];
  if (!readRecord(file_, slot, bytes)) return Result::IoError;
  PokemonInstance pokemon;
  if (!decodePokemonRecord(bytes, pokemon) || !isValidPokemon(pokemon)) return Result::Invalid;
  output = pokemon;
  return Result::Ok;
}
Result Snapshot::findEmpty(uint32_t& slot) const {
  if (!mounted) return Result::NotMounted;
  if (!ready_) return Result::Invalid;
  for (uint32_t i = 0; i < BOX_CAPACITY; ++i)
    if (!boxOccupied(bitmap_, i)) { slot = i; return Result::Ok; }
  return Result::NotFound;
}
Result Snapshot::findInstance(uint32_t id, uint32_t& slot) {
  if (!id) return Result::InvalidArgument;
  if (!mounted) return Result::NotMounted;
  if (!ready_) return Result::Invalid;
  for (uint32_t i = 0; i < BOX_CAPACITY; ++i) {
    if (!boxOccupied(bitmap_, i)) continue;
    PokemonInstance pokemon;
    Result result = readSlot(i, pokemon);
    if (result != Result::Ok) return result;
    if (pokemon.instanceId == id) { slot = i; return Result::Ok; }
  }
  return Result::NotFound;
}
Result Snapshot::findSpecies(uint16_t species, uint32_t startSlot, uint32_t& slot) {
  if (!mounted) return Result::NotMounted;
  if (!ready_) return Result::Invalid;
  if (!species || startSlot > BOX_CAPACITY) return Result::InvalidArgument;
  for (uint32_t i = startSlot; i < BOX_CAPACITY; ++i) {
    if (!boxOccupied(bitmap_, i)) continue;
    PokemonInstance pokemon;
    Result result = readSlot(i, pokemon);
    if (result != Result::Ok) return result;
    if (pokemon.speciesId == species) { slot = i; return Result::Ok; }
  }
  return Result::NotFound;
}
Result validate(BoxKey key, BoxMetadata& output) {
  Snapshot reader;
  Result result = reader.open(key);
  if (result == Result::Ok) output = reader.metadata();
  return result;
}
namespace {
Result writeSnapshot(BoxKey key, Snapshot* source, const BoxMutation* mutation) {
  bool found = false;
  Result result = exists(key, found);
  if (result != Result::Ok) return result;
  if (found) return Result::Collision;
  uint8_t bitmap[BOX_BITMAP_SIZE] = {};
  BoxMetadata metadata;
  metadata.key = key;
  if (source) {
    for (uint32_t slot = 0; slot < BOX_CAPACITY; ++slot) {
      bool used = false;
      result = source->occupied(slot, used);
      if (result != Result::Ok) return result;
      setBoxOccupied(bitmap, slot, used);
    }
    setBoxOccupied(bitmap, mutation->slot, mutation->kind != BoxMutationKind::Remove);
  }
  for (uint32_t slot = 0; slot < BOX_CAPACITY; ++slot)
    if (boxOccupied(bitmap, slot)) ++metadata.occupiedCount;
  uint8_t header[BOX_HEADER_SIZE];
  encodeBoxHeader(metadata, header);
  char path[PATH_SIZE];
  snapshotPath(key, path, sizeof(path));
  if (!LittleFS.exists("/pokemon") && !LittleFS.mkdir("/pokemon")) return Result::IoError;
  fs::File destination = LittleFS.open(path, "w");
  if (!destination) return Result::IoError;
  if (destination.write(header, sizeof(header)) != sizeof(header) ||
      destination.write(bitmap, sizeof(bitmap)) != sizeof(bitmap)) return Result::IoError;
  uint32_t crc = updateCrc32(headerCrc(header), bitmap, sizeof(bitmap));
  for (uint32_t slot = 0; slot < BOX_CAPACITY; ++slot) {
    uint8_t bytes[POKEMON_RECORD_SIZE] = {};
    if (boxOccupied(bitmap, slot)) {
      PokemonInstance pokemon;
      if (mutation && slot == mutation->slot) pokemon = mutation->pokemon;
      else {
        result = source->readSlot(slot, pokemon);
        if (result != Result::Ok) return result;
      }
      encodePokemonRecord(pokemon, bytes);
    }
    crc = updateCrc32(crc, bytes, sizeof(bytes));
    if (destination.write(bytes, sizeof(bytes)) != sizeof(bytes)) return Result::IoError;
  }
  metadata.crc32 = ~crc;
  encodeBoxHeader(metadata, header);
  if (!destination.seek(0) || destination.write(header, sizeof(header)) != sizeof(header))
    return Result::IoError;
  // Arduino flush/close have no error result. Reopen and fully verify before
  // reporting success; stronger durability/commit handling belongs to v5.
  destination.flush();
  destination.close();
  BoxMetadata verified;
  result = validate(key, verified);
  if (result != Result::Ok) return result;
  return verified.crc32 == metadata.crc32 && verified.occupiedCount == metadata.occupiedCount
    ? Result::Ok : Result::Invalid;
}
}
Result createEmpty(BoxKey key) { return writeSnapshot(key, nullptr, nullptr); }
Result mutate(BoxKey sourceKey, uint64_t newGeneration, const BoxMutation& mutation) {
  if (!newGeneration || newGeneration <= sourceKey.generation || mutation.slot >= BOX_CAPACITY ||
      (mutation.kind != BoxMutationKind::Insert && mutation.kind != BoxMutationKind::Remove &&
       mutation.kind != BoxMutationKind::Replace)) return Result::InvalidArgument;
  Snapshot source;
  Result result = source.open(sourceKey);
  if (result != Result::Ok) return result;
  bool used = false;
  result = source.occupied(mutation.slot, used);
  if (result != Result::Ok) return result;
  if (mutation.kind == BoxMutationKind::Insert && used) return Result::SlotOccupied;
  if (mutation.kind != BoxMutationKind::Insert && !used) return Result::EmptySlot;
  if (mutation.kind != BoxMutationKind::Remove) {
    if (!isValidPokemon(mutation.pokemon)) return Result::InvalidArgument;
    uint32_t existing = 0;
    result = source.findInstance(mutation.pokemon.instanceId, existing);
    if (result == Result::Ok && existing != mutation.slot) return Result::DuplicateId;
    if (result != Result::Ok && result != Result::NotFound) return result;
  }
  return writeSnapshot({sourceKey.storeId, newGeneration}, &source, &mutation);
}
}
