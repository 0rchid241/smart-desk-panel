#include "box_storage.h"
#include "game_state.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace PokemonGame;
using namespace BoxStorage;
namespace {
std::string path(BoxKey key) {
  char result[PATH_SIZE];
  assert(snapshotPath(key, result, sizeof(result)));
  return result;
}
FakeLittleFS::Bytes& bytes(BoxKey key) { return *FakeLittleFS::files.at(path(key)); }
// Independent bitwise reference, including header reserved bytes, excluding CRC.
void fixCrc(FakeLittleFS::Bytes& data) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < data.size(); ++i) {
    if (i >= 44 && i < 48) continue;
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
  }
  crc = ~crc;
  for (unsigned i = 0; i < 4; ++i) data[44 + i] = static_cast<uint8_t>(crc >> (8 * i));
}
void equalPokemon(const PokemonInstance& a, const PokemonInstance& b) {
  assert(a.instanceId == b.instanceId && a.speciesId == b.speciesId && a.exp == b.exp);
  assert(a.currentHp == b.currentHp && a.level == b.level && a.friendship == b.friendship);
  assert(a.formId == b.formId && a.gender == b.gender && a.shiny == b.shiny);
  for (unsigned i = 0; i < 4; ++i) assert(a.moves[i] == b.moves[i]);
}
}
int main() {
  static_assert(BOX_FILE_SIZE == 49472, "wire size");
  FakeLittleFS::reset();
  bool found = true;
  assert(exists({1, 1}, found) == Result::NotMounted);
  FakeLittleFS::failMount = true;
  assert(mount() == Result::IoError && !FakeLittleFS::formatRequested);
  FakeLittleFS::failMount = false;
  assert(mount() == Result::Ok);
  assert(createEmpty({0, 1}) == Result::InvalidArgument);
  FakeLittleFS::failMkdir = true;
  assert(createEmpty({1, 1}) == Result::IoError);
  FakeLittleFS::failMkdir = false;
  FakeLittleFS::failOpen = true;
  assert(createEmpty({1, 1}) == Result::IoError);
  FakeLittleFS::failOpen = false;
  const BoxKey empty{0x123456789abcdef0ULL, 1}, one{empty.storeId, 2}, two{empty.storeId, 3};
  assert(createEmpty(empty) == Result::Ok);
  assert(bytes(empty).size() == BOX_FILE_SIZE);
  assert(createEmpty(empty) == Result::Collision);
  const auto original = bytes(empty);
  assert(std::memcmp(original.data(), "PKBX\x01\x00\x40\x00\x18\x00", 10) == 0);
  assert(original[24] == 0xf0 && original[31] == 0x12);
  Snapshot reader;
  assert(reader.open(empty) == Result::Ok && reader.metadata().occupiedCount == 0);
  uint32_t slot = 999;
  assert(reader.findEmpty(slot) == Result::Ok && slot == 0);
  PokemonInstance unchanged; unchanged.instanceId = 999;
  assert(reader.readSlot(0, unchanged) == Result::EmptySlot && unchanged.instanceId == 999);
  assert(reader.occupied(2048, found) == Result::InvalidArgument);
  auto pokemon = createNewGame().party.members[0];
  pokemon.instanceId = 0x12345678; pokemon.exp = 0x1234abcd;
  pokemon.shiny = true; pokemon.gender = Gender::Female; pokemon.currentHp = 7;
  BoxMutation mutation{BoxMutationKind::Insert, 0, pokemon};
  assert(mutate(empty, 2, mutation) == Result::Ok);
  assert(bytes(empty) == original);
  assert(reader.open(one) == Result::Ok && reader.metadata().occupiedCount == 1);
  PokemonInstance actual;
  assert(reader.readSlot(0, actual) == Result::Ok); equalPokemon(pokemon, actual);
  assert(reader.findEmpty(slot) == Result::Ok && slot == 1);
  assert(reader.findInstance(pokemon.instanceId, slot) == Result::Ok && slot == 0);
  assert(reader.findInstance(99, slot) == Result::NotFound);
  assert(reader.findSpecies(25, 0, slot) == Result::Ok && slot == 0);
  assert(reader.findSpecies(25, 1, slot) == Result::NotFound);
  mutation.slot = 2047;
  assert(mutate(one, 3, mutation) == Result::DuplicateId);
  ++mutation.pokemon.instanceId;
  assert(mutate(one, 3, mutation) == Result::Ok);
  assert(reader.open(two) == Result::Ok && reader.metadata().occupiedCount == 2);
  assert(reader.findSpecies(25, 1, slot) == Result::Ok && slot == 2047);
  const auto twoOriginal = bytes(two);
  assert(mutate(one, 3, mutation) == Result::Collision && bytes(two) == twoOriginal);
  mutation.kind = BoxMutationKind::Replace;
  mutation.pokemon.gender = Gender::Genderless; mutation.pokemon.shiny = false;
  assert(mutate(two, 4, mutation) == Result::Ok);
  assert(reader.open({empty.storeId, 4}) == Result::Ok);
  assert(reader.readSlot(2047, actual) == Result::Ok); equalPokemon(mutation.pokemon, actual);
  mutation.kind = BoxMutationKind::Remove;
  assert(mutate({empty.storeId, 4}, 5, mutation) == Result::Ok);
  assert(reader.open({empty.storeId, 5}) == Result::Ok && reader.metadata().occupiedCount == 1);
  assert(reader.readSlot(2047, actual) == Result::EmptySlot);
  const auto& removed = bytes({empty.storeId, 5});
  for (size_t i = removed.size() - 24; i < removed.size(); ++i) assert(removed[i] == 0);
  assert(mutate(one, 1, mutation) == Result::InvalidArgument);
  assert(mutate(one, 7, mutation) == Result::EmptySlot);
  mutation.kind = BoxMutationKind::Insert; mutation.slot = 0;
  assert(mutate(one, 7, mutation) == Result::SlotOccupied);
  mutation.slot = 1; mutation.pokemon.instanceId = 0;
  assert(mutate(one, 7, mutation) == Result::InvalidArgument);

  // Corruption checks, including semantic damage with a freshly correct CRC.
  BoxMetadata metadata;
  for (size_t offset : {size_t(0), size_t(4), size_t(6), size_t(8), size_t(10), size_t(12),
                        size_t(16), size_t(20), size_t(24), size_t(32), size_t(40), size_t(48),
                        BOX_HEADER_SIZE, BOX_RECORDS_OFFSET + 20, BOX_RECORDS_OFFSET + 23}) {
    bytes(two) = twoOriginal;
    bytes(two)[offset] ^= 0x80;
    assert(validate(two, metadata) != Result::Ok);
    fixCrc(bytes(two));
    assert(validate(two, metadata) != Result::Ok);
  }
  bytes(two) = twoOriginal; bytes(two)[44] ^= 1;
  assert(validate(two, metadata) == Result::Invalid);
  assert(mutate(two, 8, mutation) == Result::Invalid);
  assert(exists({empty.storeId, 8}, found) == Result::Ok && !found);
  bytes(two) = twoOriginal; bytes(two).pop_back();
  assert(validate(two, metadata) == Result::Invalid);
  bytes(two) = twoOriginal; bytes(two).push_back(0);
  assert(validate(two, metadata) == Result::Invalid);
  bytes(two) = twoOriginal;
  std::memcpy(bytes(two).data() + BOX_RECORDS_OFFSET + 2047 * 24,
              bytes(two).data() + BOX_RECORDS_OFFSET, 24);
  fixCrc(bytes(two));
  assert(validate(two, metadata) == Result::DuplicateId);
  assert(mutate(two, 8, mutation) == Result::DuplicateId);
  assert(exists({empty.storeId, 8}, found) == Result::Ok && !found);
  bytes(two) = twoOriginal;
  bytes(two)[BOX_RECORDS_OFFSET + 24] = 1; // Noncanonical empty record.
  fixCrc(bytes(two)); assert(validate(two, metadata) == Result::Invalid);
  bytes(two) = twoOriginal;
  std::memset(bytes(two).data() + BOX_RECORDS_OFFSET, 0, 4);
  fixCrc(bytes(two)); assert(validate(two, metadata) == Result::Invalid);
  bytes(two) = twoOriginal;

  // Full-capacity wire fixture (host memory only), including duplicate IDs
  // within a group and across groups. No index in production storage code.
  const BoxKey full{empty.storeId, 100};
  assert(createEmpty(full) == Result::Ok);
  auto& fullBytes = bytes(full);
  BoxMetadata fullMetadata{full, BOX_CAPACITY, 0};
  encodeBoxHeader(fullMetadata, fullBytes.data());
  std::memset(fullBytes.data() + BOX_HEADER_SIZE, 0xff, BOX_BITMAP_SIZE);
  for (uint32_t i = 0; i < BOX_CAPACITY; ++i) {
    auto member = pokemon; member.instanceId = i + 1;
    encodePokemonRecord(member, fullBytes.data() + BOX_RECORDS_OFFSET + i * 24);
  }
  fixCrc(fullBytes);
  assert(reader.open(full) == Result::Ok && reader.metadata().occupiedCount == 2048);
  assert(reader.findEmpty(slot) == Result::NotFound);
  assert(reader.findInstance(2048, slot) == Result::Ok && slot == 2047);
  for (uint32_t duplicateSlot : {1u, 63u, 64u, 2047u}) {
    auto member = pokemon; member.instanceId = 1;
    encodePokemonRecord(member, fullBytes.data() + BOX_RECORDS_OFFSET + duplicateSlot * 24);
    fixCrc(fullBytes);
    assert(validate(full, metadata) == Result::DuplicateId);
    member.instanceId = duplicateSlot + 1;
    encodePokemonRecord(member, fullBytes.data() + BOX_RECORDS_OFFSET + duplicateSlot * 24);
  }
  fixCrc(fullBytes);
  assert(validate(full, metadata) == Result::Ok);

  mutation = {BoxMutationKind::Insert, 1, pokemon};
  mutation.pokemon.instanceId = 77;
  uint64_t generation = 10;
  for (size_t budget : {size_t(0), size_t(63), size_t(319), size_t(BOX_FILE_SIZE - 1), BOX_FILE_SIZE}) {
    FakeLittleFS::writeBudget = budget;
    assert(mutate(two, generation++, mutation) == Result::IoError);
    assert(bytes(two) == twoOriginal);
  }
  FakeLittleFS::writeBudget = std::numeric_limits<size_t>::max();
  FakeLittleFS::corruptFlush = true;
  assert(mutate(two, generation++, mutation) == Result::Invalid);
  FakeLittleFS::truncateClose = true;
  assert(mutate(two, generation++, mutation) == Result::Invalid);
  FakeLittleFS::failReopen = true;
  assert(mutate(two, generation++, mutation) == Result::IoError);
  FakeLittleFS::failOpen = false;
  assert(bytes(two) == twoOriginal);
  // A failed destination is never silently reused on retry.
  assert(exists({empty.storeId, 10}, found) == Result::Ok && found);
  assert(mutate(two, 10, mutation) == Result::Collision);
  FakeLittleFS::readBudget = 10;
  assert(validate(two, metadata) == Result::IoError);
  FakeLittleFS::readBudget = std::numeric_limits<size_t>::max();
  FakeLittleFS::failSeek = true;
  assert(validate(two, metadata) == Result::IoError);
  FakeLittleFS::failSeek = false;
  assert(validate(two, metadata) == Result::Ok);
  reader.close();
  FakeLittleFS::failRemove = true;
  assert(removeSnapshot(empty) == Result::IoError);
  FakeLittleFS::failRemove = false;
  assert(removeSnapshot(empty) == Result::Ok);
  assert(removeSnapshot(empty) == Result::Missing);
  unmount();
  assert(reader.open(two) == Result::NotMounted);
  assert(mount() == Result::Ok && validate(two, metadata) == Result::Ok);
  assert(createEmpty({UINT64_MAX, UINT64_MAX}) == Result::Ok);
  assert(validate({UINT64_MAX, UINT64_MAX}, metadata) == Result::Ok);
  assert(mutate({UINT64_MAX, UINT64_MAX}, 0, mutation) == Result::InvalidArgument);
  unmount();
  std::puts("PASS Box: immutable snapshots, wire/CRC/semantics/duplicate IDs, scans, failure/readback, no format");
}
