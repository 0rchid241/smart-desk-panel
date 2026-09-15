#include "game_state.h"
#include "save_data.h"
#include "save_storage.h"
#include "Preferences.h"
#include <cassert>
#include <cstdio>
#include <limits>

using namespace PokemonGame;
using GameSaveStorage::LoadResult;

std::vector<uint8_t> encode(const GameSave& save) {
  std::vector<uint8_t> bytes(SAVE_MAX_SIZE);
  size_t size = 0;
  assert(serialize(save, bytes.data(), bytes.size(), size));
  bytes.resize(size);
  return bytes;
}

// Independent CRC reference also checks protected semantic/header mutations.
void updateCrc(std::vector<uint8_t>& bytes) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i >= 14 && i < 18) continue;
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
  }
  crc = ~crc;
  for (unsigned i = 0; i < 4; ++i) bytes[14 + i] = static_cast<uint8_t>(crc >> (8 * i));
}

void codecTests() {
  GameSave save;
  save.state = createNewGame();
  assert(isValidState(save.state));
  assert(partner(save.state)->instanceId == 1 && save.state.progress.nextInstanceId == 2);
  assert(partner(save.state)->level == 5 && partner(save.state)->currentHp == 18);
  assert(partner(save.state)->friendship == 70 && partner(save.state)->exp == 0);
  assert(dexContains(save.state.pokedex.seen, 25));
  assert(dexContains(save.state.pokedex.caught, 25));
  assert(!dexContains(save.state.pokedex.shinyCaught, 25));
  assert(!dexContains(save.state.pokedex.seen, 0));
  assert(!dexContains(save.state.pokedex.seen, 1026));
  registerCaught(save.state.pokedex, 1025, true);
  assert(dexContains(save.state.pokedex.shinyCaught, 1025));

  // All fields, 16-bit move IDs, gender/shiny packing, three unique party IDs.
  save.sequence = 0x12345678;
  save.state.party.count = 3;
  for (unsigned i = 0; i < 3; ++i) {
    save.state.party.members[i] = save.state.party.members[0];
    save.state.party.members[i].instanceId = i + 1;
    save.state.party.members[i].level = static_cast<uint8_t>(5 + i);
    save.state.party.members[i].exp = 0x12340000u + i;
    save.state.party.members[i].currentHp = static_cast<uint16_t>(i);
    save.state.party.members[i].gender = static_cast<Gender>(i);
  }
  save.state.party.members[1].shiny = true;
  registerCaught(save.state.pokedex, 25, true);
  save.state.progress.nextInstanceId = 4;
  save.state.progress.deskPetId = 2;
  save.state.progress.ballTier = 2;
  save.state.progress.masterBallCount = 7;
  save.state.progress.playTimeSeconds = 987654;
  auto bytes = encode(save);
  assert(bytes.size() == SAVE_MAX_SIZE);
  assert(bytes[0] == 'P' && bytes[4] == 1 && bytes[5] == 0);
  assert(bytes[6] == 0x78 && bytes[7] == 0x56 && bytes[8] == 0x34 && bytes[9] == 0x12);
  GameSave decoded;
  assert(deserialize(bytes.data(), bytes.size(), decoded) == DecodeResult::Ok);
  assert(encode(decoded) == bytes);
  assert(partner(decoded.state)->gender == Gender::Female && partner(decoded.state)->shiny);
  size_t written = 999;
  uint8_t tiny[3];
  assert(!serialize(save, tiny, sizeof(tiny), written) && written == 0);

  // Every single-bit corruption and every truncated length must be rejected.
  for (size_t i = 0; i < bytes.size(); ++i) {
    for (unsigned bit = 0; bit < 8; ++bit) {
      auto broken = bytes; broken[i] ^= static_cast<uint8_t>(1u << bit);
      assert(deserialize(broken.data(), broken.size(), decoded) == DecodeResult::Invalid);
      assert(encode(decoded) == bytes); // No partial application.
    }
    assert(deserialize(bytes.data(), i, decoded) == DecodeResult::Invalid);
  }
  auto broken = bytes; broken.push_back(0);
  assert(deserialize(broken.data(), broken.size(), decoded) == DecodeResult::Invalid);
  broken = bytes; broken[4] = 2; updateCrc(broken);
  assert(deserialize(broken.data(), broken.size(), decoded) == DecodeResult::UnsupportedVersion);
  broken = bytes; broken[32] = 4; updateCrc(broken); // party count
  assert(deserialize(broken.data(), broken.size(), decoded) == DecodeResult::Invalid);
  broken = bytes; broken[53] = 0; updateCrc(broken); // first level
  assert(deserialize(broken.data(), broken.size(), decoded) == DecodeResult::Invalid);
  broken = bytes; broken[56] = 0x80; updateCrc(broken); // reserved flags
  assert(deserialize(broken.data(), broken.size(), decoded) == DecodeResult::Invalid);

  GameState invalid = createNewGame();
  invalid.party.members[0].currentHp = 65535; assert(!isValidState(invalid));
  invalid = createNewGame(); invalid.progress.deskPetId = 99; assert(!isValidState(invalid));
  invalid = createNewGame(); invalid.progress.nextInstanceId = 1; assert(!isValidState(invalid));
  invalid = createNewGame(); invalid.party.members[0].formId = 1; assert(!isValidState(invalid));
  invalid = createNewGame(); invalid.party.members[0].moves[0] = 999; assert(!isValidState(invalid));
  invalid = createNewGame(); invalid.pokedex.seen[3] = 0; assert(!isValidState(invalid));
  invalid = createNewGame(); invalid.pokedex.seen[128] = 0x80; assert(!isValidState(invalid));
  invalid = createNewGame(); invalid.party.members[1] = invalid.party.members[0];
  invalid.party.count = 2; assert(!isValidState(invalid));
  std::puts("PASS core: stats, IDs, bitsets, roundtrip, corruption, truncation, validation");
}

void storageTests() {
  FakeNvs::reset();
  FakeNvs::data["schedule/existing"] = {1, 2, 3};
  FakeNvs::data["calcache/json"] = {4, 5, 6};
  GameSave save;
  assert(GameSaveStorage::load(save) == LoadResult::Missing);
  save.state = createNewGame();
  assert(GameSaveStorage::save(save) && save.sequence == 1);
  const auto first = FakeNvs::data.at("pokemon_g1/save_a");
  save.state.party.members[0].exp = 123;
  save.state.party.members[0].friendship = 88;
  save.state.party.members[0].currentHp = 7;
  assert(GameSaveStorage::save(save) && save.sequence == 2);
  assert(FakeNvs::data.at("pokemon_g1/save_a") == first);
  GameSave rebooted;
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded);
  assert(rebooted.sequence == 2 && partner(rebooted.state)->exp == 123);
  assert(partner(rebooted.state)->friendship == 88 && partner(rebooted.state)->currentHp == 7);
  const unsigned writes = FakeNvs::writes;
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded && FakeNvs::writes == writes);

  // Torn write to inactive A must retain complete B for next boot.
  FakeNvs::partialWrite = true;
  assert(!GameSaveStorage::save(rebooted) && rebooted.sequence == 2);
  FakeNvs::partialWrite = false;
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded && rebooted.sequence == 2);
  FakeNvs::corruptWrite = true;
  assert(!GameSaveStorage::save(rebooted));
  FakeNvs::corruptWrite = false;
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded && rebooted.sequence == 2);
  assert(GameSaveStorage::save(rebooted) && rebooted.sequence == 3);
  FakeNvs::data["pokemon_g1/save_a"].back() ^= 1;
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded && rebooted.sequence == 2);
  FakeNvs::data["pokemon_g1/save_b"].back() ^= 1;
  assert(GameSaveStorage::load(rebooted) == LoadResult::Invalid);
  rebooted = GameSave{}; rebooted.state = createNewGame();
  assert(GameSaveStorage::save(rebooted));
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded);

  auto future = encode(rebooted); future[4] = 2; updateCrc(future);
  FakeNvs::data["pokemon_g1/save_b"] = future;
  const auto before = FakeNvs::data;
  assert(GameSaveStorage::load(rebooted) == LoadResult::UnsupportedVersion);
  assert(!GameSaveStorage::save(rebooted) && FakeNvs::data == before);
  FakeNvs::data["pokemon_g1/save_b"].resize(SAVE_MAX_SIZE + 1);
  assert(GameSaveStorage::load(rebooted) == LoadResult::UnsupportedVersion);
  FakeNvs::data.erase("pokemon_g1/save_b");
  FakeNvs::failRead = true;
  assert(GameSaveStorage::load(rebooted) == LoadResult::StorageError);
  assert(!GameSaveStorage::save(rebooted));
  FakeNvs::failRead = false;
  FakeNvs::failOpen = true;
  assert(GameSaveStorage::load(rebooted) == LoadResult::StorageError);
  assert(!GameSaveStorage::save(rebooted));
  FakeNvs::failOpen = false;
  rebooted.sequence = std::numeric_limits<uint32_t>::max();
  FakeNvs::data["pokemon_g1/save_a"] = encode(rebooted);
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded);
  assert(!GameSaveStorage::save(rebooted));
  assert(FakeNvs::data.at("schedule/existing") == std::vector<uint8_t>({1, 2, 3}));
  assert(FakeNvs::data.at("calcache/json") == std::vector<uint8_t>({4, 5, 6}));
  std::puts("PASS storage: reboot, A/B recovery, torn write, errors, versions, namespace isolation");
}

int main() { codecTests(); storageTests(); }
