#pragma once
#include "capture_test_support.h"
#include <utility>

inline std::string browserPath(const PokemonGame::BoxRoot& root) {
  char path[BoxStorage::PATH_SIZE];
  assert(BoxStorage::snapshotPath({root.storeId,root.generation},path,sizeof(path)));
  return path;
}
inline PokemonGame::GameSave browserFresh() {
  resetStorageFakes();
  PokemonGame::GameSave save;
  assert(GameSaveStorage::load(save) == GameSaveStorage::LoadResult::Missing);
  save.state = PokemonGame::createNewGame();
  assert(initializeSave(save));
  return save;
}
inline void browserFixture(PokemonGame::GameSave& save) {
  using namespace PokemonGame;
  const uint16_t slots[] = {0,3,31,100};
  const uint32_t ids[] = {8,5,7,6};
  const uint16_t species[] = {25,19,25,16};
  BoxKey key{save.boxRoot.storeId, save.boxRoot.generation};
  for (unsigned i=0;i<4;++i) {
    auto pokemon = save.state.party.members[0];
    pokemon.instanceId = ids[i]; pokemon.speciesId = species[i]; pokemon.shiny = i==1 || i==2;
    pokemon.currentHp = 3; pokemon.gender = Gender::Female;
    pokemon.moves[0] = pokemon.moves[1] = 0;
    assert(BoxStorage::mutate(key,key.generation+1,{BoxMutationKind::Insert,slots[i],pokemon}) == BoxStorage::Result::Ok);
    ++key.generation;
    registerCaught(save.state.pokedex,pokemon.speciesId,pokemon.shiny);
  }
  BoxMetadata metadata;
  assert(BoxStorage::validate(key,metadata) == BoxStorage::Result::Ok);
  save.boxRoot = boxRootFromMetadata(metadata); save.state.progress.nextInstanceId = 9;
  assert(GameSaveStorage::save(save));
}
inline std::map<std::string, FakeLittleFS::Bytes> browserFiles() {
  std::map<std::string, FakeLittleFS::Bytes> result;
  for (const auto& file : FakeLittleFS::files) result[file.first] = *file.second;
  return result;
}
inline void browserFixCrc(FakeLittleFS::Bytes& bytes) {
  uint32_t crc=0xffffffffu;
  for (size_t i=0;i<bytes.size();++i) {
    if (i>=44 && i<48) continue;
    crc^=bytes[i];
    for (unsigned bit=0;bit<8;++bit) crc=(crc&1u) ? (crc>>1)^0xedb88320u : crc>>1;
  }
  crc=~crc;
  for (unsigned i=0;i<4;++i) bytes[44+i]=static_cast<uint8_t>(crc>>(8*i));
}
inline std::vector<uint8_t> browserSaveBytes(const PokemonGame::GameSave& save) {
  std::vector<uint8_t> bytes(PokemonGame::SAVE_MAX_SIZE); size_t length=0;
  assert(PokemonGame::serialize(save,bytes.data(),bytes.size(),length));
  bytes.resize(length); return bytes;
}
