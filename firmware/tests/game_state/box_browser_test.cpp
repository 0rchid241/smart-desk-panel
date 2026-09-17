#include "browser_test_support.h"
#include "box_browser.h"
#include <cstdio>
#include <chrono>
using namespace PokemonGame;
using namespace BoxBrowser;
std::vector<uint8_t> encode(const GameSave& save);
namespace {
void ids(const Index& index, std::initializer_list<uint32_t> expected) {
  assert(index.count() == expected.size());
  uint16_t row = 0;
  for (auto id : expected) assert(index.at(row++)->instanceId == id);
  assert(!index.at(index.count()));
}
void indexTests() {
  Index index;
  assert(index.select(View::Recent) && index.count() == 0);
  assert(!index.add({0,0,25,false}) && !index.add({1,2048,25,false}));
  assert(!index.add({1,0,0,false}) && !index.add({1,0,1026,false}));
  assert(index.add({7,400,25,true}));
  for (auto view : {View::Recent,View::Dex,View::All}) { assert(index.select(view)); ids(index,{7}); }
  assert(index.add({9,200,19,false}) && index.add({5,300,25,false}) && index.add({8,1,16,true}));
  assert(index.select(View::Recent)); ids(index,{9,8,7,5});
  assert(index.select(View::Dex)); ids(index,{8,9,5,7});
  assert(index.select(View::All)); ids(index,{8,9,5,7});
  assert(index.select(View::Species,25)); ids(index,{7,5});
  assert(index.select(View::Species,1025) && index.count()==0);
  assert(!index.select(View::Species,0) && !index.select(View::Species,1026));
  assert(index.select(View::Shiny)); ids(index,{8,7});
  index.reset(); assert(index.add({1,0,25,false}));
  assert(index.select(View::Shiny) && index.count()==0);
  // No new species content: numeric metadata exercises every future Dex boundary.
  index.reset();
  for (uint16_t species=1;species<=1025;++species) assert(index.add({species,species,species,false}));
  constexpr uint16_t ends[] = {0,151,251,386,493,649,721,809,905,1025};
  for (uint16_t gen=1;gen<=9;++gen) {
    assert(index.select(View::Generation,gen));
    assert(index.count()==ends[gen]-ends[gen-1]);
    assert(index.at(0)->speciesId==ends[gen-1]+1);
    assert(index.at(index.count()-1)->speciesId==ends[gen]);
    for (uint16_t row=0;row<index.count();++row)
      assert(index.at(row)->speciesId>ends[gen-1] && index.at(row)->speciesId<=ends[gen]);
  }
  assert(!index.select(View::Generation,0) && !index.select(View::Generation,10));
  index.reset();
  for (uint16_t slot=0;slot<BOX_CAPACITY;++slot)
    assert(index.add({BOX_CAPACITY-slot,slot,static_cast<uint16_t>(slot%1025+1),slot%2==0}));
  assert(!index.add({9999,1,25,false}));
  assert(index.select(View::Recent) && index.count()==2048);
  for (uint16_t i=0;i<2048;++i) assert(index.at(i)->instanceId==2048u-i);
  assert(index.select(View::Dex));
  for (uint16_t i=1;i<2048;++i) {
    const auto& a=*index.at(i-1); const auto& b=*index.at(i);
    assert(a.speciesId<b.speciesId || (a.speciesId==b.speciesId && a.instanceId<b.instanceId));
  }
  assert(index.select(View::Shiny) && index.count()==1024);
  assert(index.select(View::All) && index.count()==2048);
  for (uint16_t i=0;i<2048;++i) assert(index.at(i)->slot==i);
}
void storageTests() {
  auto save=browserFresh(); Browser browser;
  assert(browser.open(save.boxRoot) && browser.index().count()==0); browser.close();
  browserFixture(save);
  const auto original=encode(save); const auto nvs=FakeNvs::data;
  const auto files=browserFiles(); const auto writes=FakeNvs::writes; const auto fsWrites=FakeLittleFS::writeCalls;
  for (unsigned visit=0;visit<100;++visit) {
    assert(browser.open(save.boxRoot));
    const auto reads=FakeLittleFS::readCalls;
    assert(browser.select(View::Recent)); ids(browser.index(),{8,7,6,5});
    assert(browser.select(View::Dex)); ids(browser.index(),{6,5,7,8});
    assert(browser.select(View::All)); ids(browser.index(),{8,5,7,6});
    assert(browser.select(View::Species,25)); ids(browser.index(),{8,7});
    assert(browser.select(View::Shiny)); ids(browser.index(),{7,5});
    assert(browser.select(View::Generation,1) && browser.index().count()==4);
    assert(browser.select(View::Generation,9) && browser.index().count()==0);
    assert(FakeLittleFS::readCalls==reads); // No rescan or comparator I/O.
    assert(browser.select(View::Recent));
    PokemonInstance pokemon;
    assert(browser.read(0,pokemon) && pokemon.instanceId==8);
    assert(FakeLittleFS::readCalls==reads+1); // Exactly one record for selection.
    browser.close();
    assert(!browser.isOpen() && browser.index().count()==0 && !browser.read(0,pokemon));
  }
  assert(encode(save)==original && FakeNvs::data==nvs && FakeNvs::writes==writes);
  assert(browserFiles()==files && FakeLittleFS::writeCalls==fsWrites);
  assert(FakeLittleFS::readOpens==FakeLittleFS::readCloses);
  // Missing/corrupt/mismatched and I/O failures never leave a usable handle/index.
  auto bad=save.boxRoot; ++bad.generation; assert(!browser.open(bad));
  bad=save.boxRoot; ++bad.occupiedCount; assert(!browser.open(bad));
  bad=save.boxRoot; bad.snapshotCrc32^=1; assert(!browser.open(bad));
  auto& bytes=*FakeLittleFS::files.at(browserPath(save.boxRoot)); bytes.back()^=1;
  assert(!browser.open(save.boxRoot)); bytes.back()^=1;
  FakeLittleFS::failOpen=true; assert(!browser.open(save.boxRoot)); FakeLittleFS::failOpen=false;
  BoxStorage::unmount(); FakeLittleFS::failMount=true;
  assert(!browser.open(save.boxRoot)); FakeLittleFS::failMount=false;
  assert(browser.open(save.boxRoot)); FakeLittleFS::readBudget=0;
  PokemonInstance pokemon; assert(!browser.read(0,pokemon));
  assert(!browser.isOpen() && browser.index().count()==0);
  FakeLittleFS::readBudget=std::numeric_limits<size_t>::max();
  assert(FakeLittleFS::readOpens==FakeLittleFS::readCloses);
  assert(browserFiles()==files && FakeNvs::data==nvs && FakeNvs::writes==writes);
  assert(!FakeLittleFS::formatRequested);
  installCaptureBoxFixture(save,BOX_CAPACITY,save.boxRoot.generation+1);
  const auto start=std::chrono::steady_clock::now();
  assert(browser.open(save.boxRoot) && browser.index().count()==2048);
  const auto scanned=FakeLittleFS::readCalls;
  for (auto view : {View::Recent,View::Dex,View::All,View::Shiny}) assert(browser.select(view));
  assert(FakeLittleFS::readCalls==scanned);
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
  browser.close();
  std::printf("PASS D full Box: 2048 records, entry=%zu index=%zu bytes, host open+views=%lld ms (not device timing)\n",
    sizeof(Entry),sizeof(Index),static_cast<long long>(elapsed));
}
}
void boxBrowserTests() {
  indexTests(); storageTests();
  std::puts("PASS D browser: sorts/duplicates/all Dex generation boundaries, filters, sparse/full, 100 visits read-only, errors/close");
}
