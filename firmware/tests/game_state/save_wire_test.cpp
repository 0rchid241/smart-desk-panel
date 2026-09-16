#include "save_wire_fixture.h"
#include "save_v4_golden.h"
#include "storage_test_support.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
std::vector<uint8_t> encode(const PokemonGame::GameSave& save);
void updateCrc(std::vector<uint8_t>& bytes);
void saveWireTests() {
  using namespace PokemonGame;
  static_assert(SAVE_VERSION == 5 && SAVE_V4_MAX_SIZE == 554 && SAVE_MAX_SIZE == 586, "v5 appends root");
  GameSave decoded;
  assert(deserialize(SAVE_V4_GOLDEN, sizeof(SAVE_V4_GOLDEN), decoded) == DecodeResult::Ok);
  assert(decoded.saveVersion == 4 && decoded.boxRoot.storeId == 0 && decoded.boxRoot.capacity == 0);
  assert(encode(decoded) == encode(legacyWireInput())); // Every restored state field.
  auto bytes = encode(decoded);
  assert(bytes.size() == 586);
  GameSave roundtrip;
  assert(deserialize(bytes.data(), bytes.size(), roundtrip) == DecodeResult::Ok);
  assert(roundtrip.boxRoot.storeId == wireTestRoot().storeId);
  assert(roundtrip.boxRoot.generation == wireTestRoot().generation);
  assert(roundtrip.boxRoot.snapshotCrc32 == 0); // Zero CRC is a valid wire value.
  assert(bytes[554] == 0x88 && bytes[561] == 0x11 && bytes[562] == 0x11 && bytes[569] == 0x88);
  assert(encode(roundtrip) == bytes);
  for (size_t offset : {size_t(0), size_t(8), size_t(16), size_t(20), size_t(28), size_t(30)}) {
    auto bad = bytes;
    if (offset == 0 || offset == 8) std::memset(bad.data() + 554 + offset, 0, 8);
    else bad[554 + offset] = 0xff;
    if (offset == 20) bad[554 + offset + 1] = 0xff;
    updateCrc(bad);
    assert(deserialize(bad.data(), bad.size(), roundtrip) == DecodeResult::Invalid);
  }
  // Strip only the appended root and restore the legacy header. All 554 bytes,
  // including the original CRC, must still match the unmodified B1 golden.
  bytes.resize(554); bytes[4] = 4;
  const uint32_t payload = static_cast<uint32_t>(bytes.size() - SAVE_HEADER_SIZE);
  for (unsigned i = 0; i < 4; ++i) bytes[10 + i] = static_cast<uint8_t>(payload >> (8 * i));
  updateCrc(bytes);
  assert(std::memcmp(bytes.data(), SAVE_V4_GOLDEN, bytes.size()) == 0);
  auto invalid = legacyWireInput(); invalid.boxRoot = {};
  uint8_t buffer[SAVE_MAX_SIZE]; size_t written = 0;
  assert(!serialize(invalid, buffer, sizeof(buffer), written));
  std::puts("PASS v5 wire: 586 bytes, root LE/validation/zero CRC, v4 golden full-state and byte compatibility");
}
