#include "storage_test_support.h"
#include "capture_storage.h"
#include "box_browser.h"
#include "save_v4_golden.h"
#include <cassert>
#include <cstdio>
#include <limits>

using namespace PokemonGame;
using namespace GameSaveStorage;
std::vector<uint8_t> encode(const GameSave& save);
namespace {
constexpr uint64_t STORE = 0x123abc;
std::string path(BoxKey key) {
  char name[BoxStorage::PATH_SIZE];
  assert(BoxStorage::snapshotPath(key,name,sizeof(name)));
  return name;
}
bool present(BoxKey key) { return FakeLittleFS::files.count(path(key)) != 0; }
BoxRoot make(BoxKey key) {
  assert(BoxStorage::createEmpty(key)==BoxStorage::Result::Ok);
  BoxMetadata metadata;
  assert(BoxStorage::validate(key,metadata)==BoxStorage::Result::Ok);
  return boxRootFromMetadata(metadata);
}
void assertProtected(const char* removed) {
  for (const auto& slot : FakeNvs::data) {
    if (slot.first!="pokemon_g1/save_a" && slot.first!="pokemon_g1/save_b") continue;
    GameSave decoded;
    if (deserialize(slot.second.data(),slot.second.size(),decoded)==DecodeResult::Ok && decoded.saveVersion==5)
      assert(path({decoded.boxRoot.storeId,decoded.boxRoot.generation})!=removed);
  }
}
GameSave seed() {
  resetStorageFakes();
  assert(BoxStorage::mount()==BoxStorage::Result::Ok);
  GameSave save; save.state=createNewGame(); save.sequence=10; save.boxRoot=make({STORE,10});
  FakeNvs::data["pokemon_g1/save_a"]=encode(save);
  save.sequence=11; save.boxRoot=make({STORE,11});
  FakeNvs::data["pokemon_g1/save_b"]=encode(save);
  assert(load(save)==LoadResult::Loaded);
  FakeLittleFS::beforeRemove=assertProtected;
  return save;
}
void oldFiles() { for (uint64_t g=1;g<10;++g) make({STORE,g}); }
void rootsAndNames() {
  auto save=seed(); oldFiles();
  const auto nvs=FakeNvs::data;
  auto report=collectBoxGarbage();
  assert(report.status==GcStatus::Clean && report.removed==9 && report.kept==2);
  assert(FakeLittleFS::files.size()==2 && present({STORE,10}) && present({STORE,11}));
  assert(nvs==FakeNvs::data && validatePair(save)==LoadResult::Loaded);
  for (unsigned scenario=0;scenario<3;++scenario) {
    save=seed(); oldFiles();
    if (scenario==0) FakeNvs::data["pokemon_g1/save_a"]=encode(save); // Same root.
    if (scenario==1) FakeNvs::data.erase("pokemon_g1/save_a"); // Proven missing.
    if (scenario==2) FakeNvs::data["pokemon_g1/save_a"]={SAVE_V4_GOLDEN,SAVE_V4_GOLDEN+sizeof(SAVE_V4_GOLDEN)};
    report=collectBoxGarbage();
    assert(report.status==GcStatus::Clean && report.removed==10 && report.kept==1);
    assert(FakeLittleFS::files.size()==1 && present({STORE,11}));
  }
  save=seed();
  const auto other=make({77,1});
  auto protectExtra=make({88,1});
  const std::string canonical=path({77,1}).substr(9);
  for (const std::string& name : {std::string("notes.txt"), canonical+".tmp",
      std::string("box_000000000000004D_0000000000000001.bin"),
      std::string("box_0000000000000000_0000000000000001.bin"),
      std::string("box_1_0000000000000001.bin"),std::string("BOX_000000000000004d_0000000000000001.bin")})
    FakeLittleFS::files["/pokemon/"+name]=std::make_shared<FakeLittleFS::Bytes>(size_t{3},uint8_t{1});
  FakeLittleFS::directories.insert("/pokemon/"+path({99,1}).substr(9)); // A snapshot-looking directory.
  FakeLittleFS::directories.insert("/pokemon/nested");
  FakeLittleFS::files["/pokemon/nested/"+canonical]=std::make_shared<FakeLittleFS::Bytes>();
  FakeLittleFS::files["/other/"+canonical]=std::make_shared<FakeLittleFS::Bytes>();
  const auto before=FakeLittleFS::files.size();
  report=collectBoxGarbage(&protectExtra);
  assert(report.removed==1 && !present({other.storeId,other.generation}) && present({88,1}));
  assert(FakeLittleFS::files.size()==before-1);
  BoxKey parsed{7,7};
  assert(BoxStorage::parseSnapshotName(canonical.c_str(),parsed) && parsed.storeId==77 && parsed.generation==1);
  for (const char* bad : {"", "box_0000000000000001_0000000000000000.bin",
      "box_0000000000000001_000000000000000g.bin", "box_0000000000000001_0000000000000001.bin/",
      "/pokemon/box_0000000000000001_0000000000000001.bin"})
    assert(!BoxStorage::parseSnapshotName(bad,parsed));
  assert(!BoxStorage::parseSnapshotName(nullptr,parsed));
  // Streaming scan + bounded deletion batch; small/partial orphan counts need not fit flash-size division.
  save=seed();
  for (uint64_t g=20;g<60;++g) FakeLittleFS::files[path({STORE,g})]=std::make_shared<FakeLittleFS::Bytes>();
  report=collectBoxGarbage(); assert(report.status==GcStatus::Partial && report.removed==16 && report.deferred==24);
  assert(collectBoxGarbage().removed==16);
  assert(collectBoxGarbage().removed==8 && FakeLittleFS::files.size()==2);
  std::puts("PASS GC roots: A/B, same, missing, legacy, live root, other store; strict filenames/no recursion; bounded batches");
}
void unsafeAndIo() {
  for (unsigned failure=0;failure<9;++failure) {
    auto save=seed(); oldFiles();
    const auto before=FakeLittleFS::files;
    if (failure==0) FakeNvs::data["pokemon_g1/save_a"].back()^=1;
    if (failure==1) FakeNvs::data["pokemon_g1/save_a"][0]^=1;
    if (failure==2) FakeNvs::data["pokemon_g1/save_a"][4]=6;
    if (failure==3) FakeNvs::failRead=true;
    if (failure==4) FakeNvs::failOpen=true;
    if (failure==5) { BoxStorage::unmount(); FakeLittleFS::failMount=true; }
    if (failure==6) FakeLittleFS::failDirOpen=true;
    if (failure==7) FakeLittleFS::directoryReadBudget=5;
    if (failure==8) FakeLittleFS::failDirClose=true;
    const auto report=collectBoxGarbage(&save.boxRoot);
    assert(report.status==(failure<5 ? GcStatus::SkippedUnsafe : GcStatus::IoError));
    assert(!report.removed && FakeLittleFS::files==before && FakeLittleFS::removeAttempts.empty());
    assert(FakeLittleFS::directoryOpens==FakeLittleFS::directoryCloses);
  }
  // A reads correctly but B errors: do not use only A's protection set.
  auto readFailure=seed(); oldFiles(); FakeNvs::readBudget=2;
  assert(collectBoxGarbage().status==GcStatus::SkippedUnsafe);
  assert(FakeLittleFS::files.size()==11 && FakeLittleFS::removeAttempts.empty());
  // Commit target readback succeeds; only the subsequent maintenance NVS read fails.
  readFailure=seed(); oldFiles(); FakeNvs::readBudget=2;
  assert(saveDetailed(readFailure)==CommitResult::Committed);
  assert(lastBoxGc().status==GcStatus::SkippedUnsafe && FakeLittleFS::files.size()==11);
  assert(canWrite());
  readFailure=seed(); oldFiles(); FakeLittleFS::directoryReadBudget=5;
  assert(saveDetailed(readFailure)==CommitResult::Committed);
  assert(lastBoxGc().status==GcStatus::IoError && FakeLittleFS::removeAttempts.empty());
  FakeLittleFS::directoryReadBudget=5;
  assert(load(readFailure)==LoadResult::Loaded && lastBoxGc().status==GcStatus::IoError);
  // A corrupt NVS slot may still permit load rollback, but never global GC.
  auto save=seed(); oldFiles(); FakeNvs::data["pokemon_g1/save_a"].back()^=1;
  assert(load(save)==LoadResult::Loaded && lastBoxGc().status==GcStatus::SkippedUnsafe);
  assert(FakeLittleFS::files.size()==11);
  assert(saveDetailed(save)==CommitResult::Committed); // Repairs the corrupt target.
  assert(lastBoxGc().status==GcStatus::Clean && FakeLittleFS::files.size()==1);
  // Decoded references are protected even if that pair's snapshot is damaged.
  save=seed(); oldFiles(); FakeLittleFS::files.at(path({STORE,10}))->back()^=1;
  assert(collectBoxGarbage().kept==2 && present({STORE,10}));
  for (size_t budget : {size_t{0},size_t{3}}) {
    save=seed(); oldFiles(); FakeLittleFS::removeBudget=budget;
    ++save.state.progress.playTimeSeconds;
    assert(saveDetailed(save)==CommitResult::Committed);
    assert(lastBoxGc().status==(budget ? GcStatus::Partial : GcStatus::IoError));
    assert(present({STORE,11}) && canWrite());
    FakeLittleFS::removeBudget=std::numeric_limits<size_t>::max();
    assert(load(save)==LoadResult::Loaded && FakeLittleFS::files.size()==1);
  }
  std::puts("PASS GC unsafe: corrupt/future/I/O/unknown skip; complete enumeration+close before delete; cleanup failure never rolls back commit");
}
void uncertaintyAndPowerLoss() {
  for (bool committed : {false,true}) {
    auto save=seed(); auto candidate=save; candidate.boxRoot=make({STORE,12});
    if (committed) FakeNvs::failReadAfterWrite=true;
    else { FakeNvs::rejectWrite=true; FakeNvs::failRead=true; }
    assert(saveDetailed(candidate)==CommitResult::Indeterminate);
    FakeNvs::failReadAfterWrite=FakeNvs::failRead=FakeNvs::rejectWrite=false;
    const auto files=FakeLittleFS::files;
    assert(collectBoxGarbage().status==GcStatus::SkippedUnsafe && FakeLittleFS::files==files);
    assert(!canWrite() && !snapshotUnreferenced({STORE,12}));
    assert(saveDetailed(candidate)==CommitResult::Indeterminate);
    assert(load(save)==LoadResult::Loaded && canWrite());
    assert(save.boxRoot.generation==(committed ? 12u : 11u));
    assert(present({STORE,12})==committed && present({STORE,11}));
  }
  auto save=seed(); oldFiles();
  struct PowerLoss {};
  unsigned deletions=0;
  FakeLittleFS::beforeRemove=[&](const char* name) {
    assertProtected(name);
    if (deletions++==3) throw PowerLoss{};
  };
  try { collectBoxGarbage(); assert(false); } catch (const PowerLoss&) {}
  assert(FakeLittleFS::files.size()==8 && present({STORE,10}) && present({STORE,11}));
  FakeLittleFS::beforeRemove=assertProtected;
  assert(load(save)==LoadResult::Loaded && save.boxRoot.generation==11);
  assert(FakeLittleFS::files.size()==2 && present({STORE,10}));
  // The open current-root Browser remains valid during maintenance.
  BoxBrowser::Browser browser;
  assert(browser.open(save.boxRoot)); oldFiles();
  assert(collectBoxGarbage().removed==9 && browser.isOpen());
  assert(browser.select(BoxBrowser::View::All)); browser.close();
  std::puts("PASS GC uncertainty: latch/no delete, reboot committed/old-pair recovery; power loss after three removals; open Browser root retained");
}
GameSave captureReady() {
  auto save=seed();
  while (save.state.party.count<PARTY_CAPACITY) {
    auto pokemon=save.state.party.members[0]; pokemon.instanceId=save.state.progress.nextInstanceId++;
    save.state.party.members[save.state.party.count++]=pokemon;
  }
  assert(setEncounter(save.state.encounter,19,0,5,Gender::Female,false));
  assert(startBattle(save.state)); save.state.progress.masterBallCount=2;
  assert(GameSaveStorage::save(save));
  return save;
}
void exactCleanupAndSteadyState() {
  auto save=captureReady(); const auto before=encode(save); const auto nvs=FakeNvs::data;
  BattleCaptureReport report;
  FakeNvs::rejectWrite=true;
  for (unsigned attempt=0;attempt<12;++attempt) {
    assert(CaptureStorage::attempt(save,CaptureBall::Master,report)==CaptureStorage::Result::NotCommitted);
    assert(!present({STORE,12}) && encode(save)==before && nvs==FakeNvs::data && !report.captured);
  }
  FakeNvs::rejectWrite=false;
  assert(CaptureStorage::attempt(save,CaptureBall::Master,report)==CaptureStorage::Result::Committed);
  assert(save.boxRoot.generation==12);
  for (unsigned failure=0;failure<5;++failure) {
    save=captureReady(); const auto bytes=encode(save);
    if (failure==0) FakeLittleFS::writeBudget=100;
    if (failure==1) FakeLittleFS::failReopen=true;
    if (failure==2) FakeLittleFS::truncateClose=true;
    if (failure==3) FakeNvs::partialWrite=true;
    if (failure==4) { FakeNvs::rejectWrite=true; FakeLittleFS::failRemove=true; }
    const auto result=CaptureStorage::attempt(save,CaptureBall::Master,report);
    assert(result==(failure<3 ? CaptureStorage::Result::StorageError : CaptureStorage::Result::NotCommitted));
    assert(encode(save)==bytes && present({STORE,11}));
    assert(present({STORE,12})==(failure==4));
  }
  save=captureReady();
  for (unsigned capture=0;capture<24;++capture) {
    assert(CaptureStorage::attempt(save,CaptureBall::Master,report)==CaptureStorage::Result::Committed);
    assert(FakeLittleFS::files.size()==2);
    assert(GameSaveStorage::save(save)); // Both slots now reference the same root.
    assert(FakeLittleFS::files.size()==1);
    assert(setEncounter(save.state.encounter,19,0,5,Gender::Female,false));
    assert(startBattle(save.state)); save.state.progress.masterBallCount=2;
    assert(GameSaveStorage::save(save));
  }
  assert(save.boxRoot.occupiedCount==24);
  assert(FakeLittleFS::formats==0 && !FakeLittleFS::formatRequested);
  std::puts("PASS GC exact cleanup: 12 NotCommitted retries reuse generation; partial/flush/reopen/torn target cleanup; remove failure preserves live; 24 captures stay at 1-2 snapshots");
}
}
void boxGcTests() { rootsAndNames(); unsafeAndIo(); uncertaintyAndPowerLoss(); exactCleanupAndSteadyState(); }
