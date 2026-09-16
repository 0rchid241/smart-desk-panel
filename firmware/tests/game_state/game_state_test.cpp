#include "storage_test_support.h"
#include "game_state.h"
#include "save_data.h"
#include "save_storage.h"
#include "Preferences.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>

using namespace PokemonGame;
using GameSaveStorage::LoadResult;

std::vector<uint8_t> encode(const GameSave& save) {
  std::vector<uint8_t> bytes(SAVE_MAX_SIZE);
  size_t size = 0;
  auto current = save;
  current.saveVersion = SAVE_VERSION;
  if (!current.boxRoot.storeId) current.boxRoot = wireTestRoot();
  assert(serialize(current, bytes.data(), bytes.size(), size));
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
  assert(bytes[0] == 'P' && bytes[4] == SAVE_VERSION && bytes[5] == 0);
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
  broken = bytes; broken[4] = SAVE_VERSION + 1; updateCrc(broken);
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
  resetStorageFakes();
  FakeNvs::data["schedule/existing"] = {1, 2, 3};
  FakeNvs::data["calcache/json"] = {4, 5, 6};
  GameSave save;
  assert(GameSaveStorage::load(save) == LoadResult::Missing);
  save.state = createNewGame();
  assert(initializeSave(save) && save.sequence == 1);
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
  assert(GameSaveStorage::load(rebooted) == LoadResult::RecoveryRequired);
  const auto corruptBoth = FakeNvs::data;
  assert(!initializeSave(rebooted) && !GameSaveStorage::save(rebooted));
  assert(FakeNvs::data == corruptBoth);
  // Restore the known good backup for the remaining independent error cases.
  FakeNvs::data["pokemon_g1/save_a"] = first;
  FakeNvs::data.erase("pokemon_g1/save_b");
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded);

  auto future = encode(rebooted); future[4] = SAVE_VERSION + 1; updateCrc(future);
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

// Frozen G1 field order. Deliberately independent of the current serializer.
std::vector<uint8_t> legacyRecord(const GameSave& save) {
  std::vector<uint8_t> bytes = {'P', 'K', 'D', 'G', 1, 0};
  auto append = [&](uint32_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
  };
  append(save.sequence, 4);
  append(15 + save.state.party.count * 24 + 387, 4);
  append(0, 4);
  const auto& progress = save.state.progress;
  append(progress.nextInstanceId, 4); append(progress.deskPetId, 4);
  append(progress.ballTier, 1); append(progress.masterBallCount, 1); append(progress.playTimeSeconds, 4);
  append(save.state.party.count, 1);
  for (uint8_t i = 0; i < save.state.party.count; ++i) {
    const auto& p = save.state.party.members[i];
    append(p.instanceId, 4); append(p.speciesId, 2); append(p.exp, 4); append(p.currentHp, 2);
    for (MoveId move : p.moves) append(move, 2);
    append(p.level, 1); append(p.friendship, 1); append(p.formId, 1);
    append(static_cast<uint8_t>(p.gender) | (p.shiny ? 4u : 0u), 1);
  }
  for (const uint8_t* bits : {save.state.pokedex.seen, save.state.pokedex.caught, save.state.pokedex.shinyCaught})
    bytes.insert(bytes.end(), bits, bits + 129);
  updateCrc(bytes);
  return bytes;
}

// Frozen G2 layout: 현재 serializer와 독립적으로 G1 뒤에 15바이트를 붙인다.
std::vector<uint8_t> legacyV2Record(const GameSave& save) {
  auto bytes = legacyRecord(save);
  const auto& s = save.state.exploration;
  auto append = [&](uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
  };
  append(static_cast<uint8_t>(s.status),1); append(s.regionId,2);
  append(s.startedAtEpoch,8); append(s.durationSeconds,4);
  // save version = 2
  bytes[4] = 2;
  bytes[5] = 0;
  // payload length 다시 기록
  const uint32_t payloadLength = static_cast<uint32_t>(bytes.size() - SAVE_HEADER_SIZE);
  for (unsigned i = 0; i < 4; ++i) {
    bytes[10 + i] = static_cast<uint8_t>(payloadLength >> (8 * i));
  }
  updateCrc(bytes);
  return bytes;
}

void explorationTests() {
  constexpr uint64_t start = 1800000000ULL;
  ExplorationSession session;
  assert(!startExploration(session, TEST_REGION_ID, 0, 15));
  assert(!startExploration(session, TEST_REGION_ID, MIN_EXPLORATION_EPOCH - 1, 15));
  assert(!startExploration(session, 2, start, 15));
  assert(!startExploration(session, TEST_REGION_ID, start, 0));
  assert(!startExploration(session, TEST_REGION_ID, std::numeric_limits<uint64_t>::max(), 15));
  assert(startExploration(session, TEST_REGION_ID, start, 15));
  assert(session.regionId == TEST_REGION_ID && session.startedAtEpoch == start && session.durationSeconds == 15);
  assert(session.status == ExplorationStatus::Exploring);
  assert(!startExploration(session, TEST_REGION_ID, start + 1, 15));
  assert(remainingSeconds(session, start) == 15);
  assert(remainingSeconds(session, start + 3) == 12);
  assert(remainingSeconds(session, start + 14) == 1);
  assert(remainingSeconds(session, start + 15) == 0);
  assert(!updateExploration(session, 0) && !updateExploration(session, start - 1));
  assert(remainingSeconds(session, start - 1) == 15);
  assert(!updateExploration(session, start + 14));

  GameSave save; save.state = createNewGame(); save.state.exploration = session;
  GameSave rebooted;
  auto bytes = encode(save);
  assert(deserialize(bytes.data(), bytes.size(), rebooted) == DecodeResult::Ok);
  assert(encode(rebooted) == bytes);
  assert(!updateExploration(rebooted.state.exploration, 0)); // Offline reboot.
  assert(rebooted.state.exploration.startedAtEpoch == start);
  assert(updateExploration(rebooted.state.exploration, start + 15));
  assert(rebooted.state.exploration.status == ExplorationStatus::Complete);
  assert(!updateExploration(rebooted.state.exploration, start + 9999));
  bytes = encode(rebooted);
  assert(deserialize(bytes.data(), bytes.size(), save) == DecodeResult::Ok);
  assert(save.state.exploration.status == ExplorationStatus::Complete);
  assert(acknowledgeExploration(save.state.exploration));
  assert(save.state.exploration.status == ExplorationStatus::Idle && save.state.exploration.startedAtEpoch == 0);
  assert(!acknowledgeExploration(save.state.exploration));
  // Full 64-bit epochs survive serialization; future duration > 60s is supported.
  assert(startExploration(save.state.exploration, TEST_REGION_ID, 0x123456789ULL, 80));
  bytes = encode(save);
  assert(deserialize(bytes.data(), bytes.size(), rebooted) == DecodeResult::Ok);
  assert(rebooted.state.exploration.startedAtEpoch == 0x123456789ULL);
  assert(remainingSeconds(rebooted.state.exploration, 0x123456789ULL + 1) == 79);
  // Correct CRC does not bypass exploration field validation.
  for (size_t offset : {size_t(0), size_t(1), size_t(3), size_t(11)}) {
    auto invalid = bytes;
    const size_t position = bytes.size() - BOX_ROOT_RECORD_SIZE - BATTLE_RECORD_SIZE - ENCOUNTER_RECORD_SIZE - EXPLORATION_RECORD_SIZE;
    if (offset == 0) invalid[position] = 9;
    if (offset == 1) invalid[position + 1] = 2;
    if (offset == 3) for (size_t i = 0; i < 8; ++i) invalid[position + 3 + i] = 0;
    if (offset == 11) for (size_t i = 0; i < 4; ++i) invalid[position + 11 + i] = 0;
    updateCrc(invalid);
    assert(deserialize(invalid.data(), invalid.size(), rebooted) == DecodeResult::Invalid);
  }
  std::puts("PASS exploration: start, elapsed, offline/rollback, completion once, ack, 64-bit persistence");
}

void encounterTests() {
  EncounterState encounter;

  // 기본 상태
  assert(encounter.status == EncounterStatus::None);
  assert(isValidEncounter(encounter));

  // None 상태에서 다른 값이 남아 있으면 invalid
  EncounterState invalidNone;
  invalidNone.gender = Gender::Male;
  assert(!isValidEncounter(invalidNone));

  // 잘못된 조우 생성
  assert(!setEncounter(encounter, 0, 0, 5, Gender::Male, false));
  assert(!setEncounter(encounter, 1026, 0, 5, Gender::Male, false));
  assert(!setEncounter(encounter, 19, 0, 0, Gender::Male, false));
  assert(!setEncounter(encounter, 19, 0, 101, Gender::Male, false));

  // 정상 야생 포켓몬 조우 생성
  assert(setEncounter(encounter, 19, 0, 3, Gender::Female, true));
  assert(encounter.status == EncounterStatus::Ready);
  assert(encounter.speciesId == 19);
  assert(encounter.formId == 0);
  assert(encounter.level == 3);
  assert(encounter.gender == Gender::Female);
  assert(encounter.shiny);
  assert(isValidEncounter(encounter));

  // save -> load 후 동일한 조우 유지
  GameSave save;
  save.state = createNewGame();
  save.state.encounter = encounter;

  const auto bytes = encode(save);

  GameSave decoded;
  assert(
    deserialize(bytes.data(), bytes.size(), decoded) ==
    DecodeResult::Ok
  );

  assert(decoded.state.encounter.status == EncounterStatus::Ready);
  assert(decoded.state.encounter.speciesId == 19);
  assert(decoded.state.encounter.formId == 0);
  assert(decoded.state.encounter.level == 3);
  assert(decoded.state.encounter.gender == Gender::Female);
  assert(decoded.state.encounter.shiny);

  // CRC가 정상이어도 의미적으로 잘못된 Encounter는 거부
  const size_t position =
    bytes.size() - BOX_ROOT_RECORD_SIZE - BATTLE_RECORD_SIZE - ENCOUNTER_RECORD_SIZE;

  for (int test = 0; test < 5; ++test) {
    auto invalid = bytes;

    if (test == 0) {
      invalid[position] = 9;  // invalid status
    }

    if (test == 1) {
      invalid[position + 1] = 0;  // speciesId = 0
      invalid[position + 2] = 0;
    }

    if (test == 2) {
      invalid[position + 4] = 0;  // level = 0
    }

    if (test == 3) {
      invalid[position + 5] = 9;  // invalid gender
    }
    if (test == 4) invalid[position + 6] = 2; // bool은 0/1만 허용.

    updateCrc(invalid);

    GameSave rejected;
    assert(
      deserialize(
        invalid.data(),
        invalid.size(),
        rejected
      ) == DecodeResult::Invalid
    );
  }

  // clearEncounter 검증
  clearEncounter(encounter);

  assert(encounter.status == EncounterStatus::None);
  assert(encounter.speciesId == 0);
  assert(encounter.formId == 0);
  assert(encounter.level == 0);
  assert(encounter.gender == Gender::Genderless);
  assert(!encounter.shiny);
  assert(isValidEncounter(encounter));

  // 테스트 Encounter Resolver: roll 0 -> 꼬렛
  assert(resolveTestEncounter(encounter, 0));
  assert(encounter.speciesId == 19);
  assert(encounter.level == 2);

  // 이미 Ready 상태이면 다시 뽑히면 안 된다.
  const SpeciesId firstSpecies = encounter.speciesId;
  assert(!resolveTestEncounter(encounter, 99));
  assert(encounter.speciesId == firstSpecies);

  // 구구 fixture
  clearEncounter(encounter);
  assert(resolveTestEncounter(encounter, 60));
  assert(encounter.speciesId == 16);
  assert(encounter.level == 3);

  // 피카츄 fixture
  clearEncounter(encounter);
  assert(resolveTestEncounter(encounter, 99));
  assert(encounter.speciesId == 25);
  assert(encounter.level == 4);

  // 결과 species는 실제 species table에서 조회 가능해야 한다.
  const auto* wildSpecies =
    findSpecies(encounter.speciesId, encounter.formId);
  assert(wildSpecies != nullptr);

  // 마지막으로 다시 clear
  clearEncounter(encounter);
  assert(encounter.status == EncounterStatus::None);
  assert(encounter.speciesId == 0);
  assert(encounter.level == 0);
  assert(encounter.gender == Gender::Genderless);
  assert(!encounter.shiny);

  // 동일한 탐험은 항상 동일한 roll을 만든다.
  ExplorationSession exploration;
  exploration.status = ExplorationStatus::Complete;
  exploration.regionId = TEST_REGION_ID;
  exploration.startedAtEpoch = 1800000000ULL;
  exploration.durationSeconds = 15;

  const uint32_t roll1 =
    makeTestEncounterRoll(exploration);

  const uint32_t roll2 =
    makeTestEncounterRoll(exploration);

  assert(roll1 == roll2);

  // 다른 탐험 시작 시각은 다른 roll을 만든다.
  ExplorationSession another = exploration;
  another.startedAtEpoch++;

  const uint32_t roll3 =
    makeTestEncounterRoll(another);

  assert(roll1 != roll3);

  // 같은 roll을 사용하면 조우 결과도 동일해야 한다.
  EncounterState firstEncounter;
  EncounterState secondEncounter;

  assert(resolveTestEncounter(firstEncounter, roll1));
  assert(resolveTestEncounter(secondEncounter, roll1));

  assert(firstEncounter.speciesId == secondEncounter.speciesId);
  assert(firstEncounter.formId == secondEncounter.formId);
  assert(firstEncounter.level == secondEncounter.level);
  assert(firstEncounter.gender == secondEncounter.gender);
  assert(firstEncounter.shiny == secondEncounter.shiny);

  std::puts(
    "PASS encounter: validation, roundtrip, corruption, clear, resolver, deterministic roll"
  );
}

void migrationTests() {
  resetStorageFakes();
  GameSave original; original.state = createNewGame(); original.sequence = 41;
  original.state.party.count = 3;
  for (uint8_t i = 0; i < 3; ++i) {
    original.state.party.members[i] = original.state.party.members[0];
    auto& p = original.state.party.members[i];
    p.instanceId = 10 + i; p.level = 5 + i; p.exp = 4567 + i;
    p.friendship = 91 + i; p.currentHp = 3 + i;
    p.gender = i == 1 ? Gender::Female : Gender::Male;
    p.shiny = i == 2;
  }
  original.state.progress.deskPetId = 11; original.state.progress.nextInstanceId = 13;
  original.state.progress.ballTier = 2; original.state.progress.masterBallCount = 3;
  original.state.progress.playTimeSeconds = 987;
  registerCaught(original.state.pokedex, 25, true);
  registerCaught(original.state.pokedex, 1025, true);
  const auto g1 = legacyRecord(original);
  assert(g1.size() == 492);
  FakeNvs::data["pokemon_g1/save_a"] = g1;
  GameSave loaded;
  assert(GameSaveStorage::load(loaded) == LoadResult::Loaded && loaded.saveVersion == 1);
  assert(loaded.state.exploration.status == ExplorationStatus::Idle);
  auto migrated = loaded; migrated.saveVersion = SAVE_VERSION;
  assert(encode(migrated) == encode(original)); // All former fields, not just the partner.
  FakeNvs::partialWrite = true;
  assert(!initializeSave(loaded) && loaded.saveVersion == 1 && loaded.sequence == 41);
  assert(FakeNvs::data.at("pokemon_g1/save_a") == g1);
  FakeNvs::partialWrite = false;
  assert(GameSaveStorage::load(loaded) == LoadResult::Loaded && loaded.saveVersion == 1);
  assert(initializeSave(loaded) && loaded.saveVersion == SAVE_VERSION && loaded.sequence == 42);
  assert(FakeNvs::data.at("pokemon_g1/save_a") == g1);
  assert(GameSaveStorage::load(migrated) == LoadResult::Loaded && migrated.saveVersion == SAVE_VERSION);
  assert(encode(loaded) == encode(migrated));
  // Mixed-version slots: corrupted newest v2 recovers original v1, not a new game.
  FakeNvs::data["pokemon_g1/save_b"].back() ^= 1;
  assert(GameSaveStorage::load(loaded) == LoadResult::Loaded && loaded.sequence == 41 && loaded.saveVersion == 1);
  // A higher sequence v1 must also win over a valid older v2.
  FakeNvs::data["pokemon_g1/save_b"] = encode(migrated);
  original.sequence = 43; FakeNvs::data["pokemon_g1/save_a"] = legacyRecord(original);
  assert(GameSaveStorage::load(loaded) == LoadResult::Loaded && loaded.sequence == 43 && loaded.saveVersion == 1);
  std::puts("PASS migration: G1 full party/progress/dex preserved, safe A/B upgrade and failure recovery");
}

void v2MigrationTests() {
  resetStorageFakes();
  GameSave original;
  original.state = createNewGame();

  original.sequence = 70;
  constexpr uint64_t start = 1800000000ULL;
  // G2에서 이미 존재하던 탐험 정보
  assert(startExploration( original.state.exploration, TEST_REGION_ID, start, 15));
  // 현재 v3에서는 조우가 있다고 가정하되,
  // v2 record를 만들 때 이 필드는 제거되어야 한다.
  assert(setEncounter(original.state.encounter, 19, 0, 3, Gender::Female, true));
  const auto v2 = legacyV2Record(original);
  FakeNvs::data["pokemon_g1/save_a"] = v2;
  GameSave loaded;
  // 실제 v2 세이브 로드
  assert(GameSaveStorage::load(loaded) == LoadResult::Loaded);
  assert(loaded.saveVersion == 2);
  assert(loaded.sequence == 70);
  // 기존 G2 탐험 정보는 유지
  assert(loaded.state.exploration.status == ExplorationStatus::Exploring);
  assert(loaded.state.exploration.regionId == TEST_REGION_ID);
  assert(loaded.state.exploration.startedAtEpoch == start);
  assert(loaded.state.exploration.durationSeconds == 15);
  // v2에는 Encounter가 없었으므로
  // 기본 None 상태로 시작해야 한다.
  assert(loaded.state.encounter.status == EncounterStatus::None);
  assert(isValidEncounter(loaded.state.encounter));
  // 저장하면 v3로 안전하게 이전
  assert(initializeSave(loaded));
  assert(loaded.saveVersion == SAVE_VERSION);
  assert(loaded.sequence == 71);
  // 재부팅을 가정해 다시 읽기
  GameSave rebooted;
  assert(GameSaveStorage::load(rebooted) == LoadResult::Loaded);
  assert(rebooted.saveVersion == SAVE_VERSION);
  assert(rebooted.state.exploration.status == ExplorationStatus::Exploring);
  assert(rebooted.state.exploration.startedAtEpoch == start);
  assert(rebooted.state.encounter.status == EncounterStatus::None);
  assert(rebooted.state.battle.status == BattleStatus::None);
  std::puts("PASS v2 migration: exploration preserved, empty encounter/battle, safe current-version upgrade");
}

void battleTests();
void saveWireTests();
void savePairTests();
int main() {
  saveWireTests();
  savePairTests();
  battleTests();
  codecTests();
  storageTests();
  explorationTests();
  encounterTests();
  migrationTests();
  v2MigrationTests();
}
