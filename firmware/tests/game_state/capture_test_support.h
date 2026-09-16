#pragma once
#include "storage_test_support.h"
#include <cassert>

// Host-only snapshot fixture: build full/terminal-generation Boxes without
// thousands of immutable mutations. Never used by the firmware.
inline void installCaptureBoxFixture(PokemonGame::GameSave& save, uint32_t count,
                                     uint64_t generation) {
  using namespace PokemonGame;
  BoxMetadata metadata{{save.boxRoot.storeId, generation}, count, 0};
  auto bytes = std::make_shared<FakeLittleFS::Bytes>(BOX_FILE_SIZE, uint8_t{0});
  encodeBoxHeader(metadata, bytes->data());
  for (uint32_t slot = 0; slot < count; ++slot) {
    auto pokemon = save.state.party.members[0];
    pokemon.instanceId = save.state.progress.nextInstanceId++;
    setBoxOccupied(bytes->data() + BOX_HEADER_SIZE, slot, true);
    encodePokemonRecord(pokemon, bytes->data() + BOX_RECORDS_OFFSET + slot * POKEMON_RECORD_SIZE);
  }
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < bytes->size(); ++i) {
    if (i >= BOX_CRC_OFFSET && i < BOX_CRC_OFFSET + 4) continue;
    crc ^= (*bytes)[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
  }
  metadata.crc32 = ~crc;
  encodeBoxHeader(metadata, bytes->data());
  char path[BoxStorage::PATH_SIZE];
  assert(BoxStorage::snapshotPath(metadata.key, path, sizeof(path)));
  assert(!FakeLittleFS::files.count(path));
  FakeLittleFS::files[path] = bytes;
  save.boxRoot = boxRootFromMetadata(metadata);
  assert(GameSaveStorage::save(save));
}
