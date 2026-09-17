#pragma once
#include "browser_test_support.h"
#include "game_app.h"
#include "box_details.h"
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <cstdio>

// Same interaction tests run against asset and fallback builds of the real GameApp.
inline void boxUiTests(Adafruit_SSD1306& text, Adafruit_SSD1306& graphics, bool assets) {
  using namespace PokemonGame;
  auto visible=[&](const char* value) { return text.text.find(value)!=std::string::npos; };
  auto press=[](ButtonEvent button, unsigned count=1) {
    for (unsigned i=0;i<count;++i) GameApp::handleButton(button);
  };
  auto enter=[&]() {
    GameApp::init(); GameApp::drawGameTextScreen();
    press(BUTTON_RIGHT,2); assert(visible("박스")); press(BUTTON_OK);
    assert(GameApp::boxBrowserActive());
  };
  auto search=[&]() { enter(); press(BUTTON_RIGHT,3); press(BUTTON_OK); assert(visible("도감번호")); };
  auto number=[&](const char* digits) {
    press(BUTTON_OK);
    assert(visible("도감번호 찾기"));
    for (unsigned i=0;i<4;++i) { press(BUTTON_RIGHT,static_cast<unsigned>(digits[i]-'0')); press(BUTTON_OK); }
  };
  PokemonInstance moves;
  const MoveId ids[] = {84,45,33,21};
  for (uint8_t count=0;count<=4;++count) {
    assert(BoxDetails::moveCount(moves)==count);
    assert(BoxDetails::movePages(moves)==(count>2 ? 2 : 1));
    for (uint8_t i=0;i<count;++i) assert(std::string(BoxDetails::moveName(moves,i))==findMove(ids[i])->name);
    assert(BoxDetails::moveName(moves,count)==nullptr);
    if (count<4) moves.moves[count]=ids[count];
  }
  moves.moves[0]=0; moves.moves[1]=65000; moves.moves[2]=0;
  assert(BoxDetails::moveCount(moves)==2 && std::string(BoxDetails::moveName(moves,0))=="?");
  assert(std::string(BoxDetails::moveName(moves,1))==findMove(21)->name);
  GameApp::leaveGameMode();
  auto save=browserFresh();
  const auto emptyNvs=FakeNvs::data; const auto emptyFiles=browserFiles();
  enter(); assert(visible("박스가 비어 있다"));
  press(BUTTON_RIGHT_LONG); assert(visible("박스가 비어 있다"));
  press(BUTTON_OK); assert(!GameApp::boxBrowserActive());
  assert(emptyNvs==FakeNvs::data && emptyFiles==browserFiles());

  for (unsigned menu : {0u,1u,3u}) {
    GameApp::init(); GameApp::drawGameTextScreen();
    press(BUTTON_RIGHT,menu); const auto home=text.text;
    const auto before=FakeNvs::data; const auto fileBefore=browserFiles();
    press(BUTTON_OK); assert(text.text!=home);
    press(BUTTON_BACK); assert(text.text==home);
    assert(before==FakeNvs::data && fileBefore==browserFiles());
  }
  browserFixture(save);
  const auto original=FakeNvs::data; const auto files=browserFiles();
  const auto writes=FakeNvs::writes; const auto fsWrites=FakeLittleFS::writeCalls;
  const uint32_t expected[3][4]={{8,7,6,5},{6,5,7,8},{8,5,7,6}};
  for (unsigned view=0;view<3;++view) {
    enter(); assert(visible("최근 포획") && visible("도감순 보기"));
    press(BUTTON_RIGHT,view); press(BUTTON_OK);
    assert(visible("1/4"));
    press(BUTTON_RIGHT_LONG); assert(visible("3/4")); // Exactly 10 Pokemon, skipping back action.
    press(BUTTON_LEFT_LONG); assert(visible("1/4"));
    unsigned rows=0;
    for (const auto& run : text.textRuns) if (run.x==12 && (run.y==16 || run.y==32 || run.y==48)) ++rows;
    assert(rows==3);
    const auto reads=FakeLittleFS::readCalls;
    const auto bitmaps=graphics.bitmaps;
    for (unsigned redraw=0;redraw<20;++redraw) { GameApp::drawGameTextScreen(); GameApp::drawGameGraphics(); }
    assert(FakeLittleFS::readCalls==reads); // Cached selected record; rows use metadata.
    if (assets) assert(graphics.bitmaps>bitmaps);
    else assert(graphics.text.find('?')!=std::string::npos);
    for (unsigned row=0;row<4;++row) {
      PokemonInstance selected;
      const uint32_t id=expected[view][row];
      const uint16_t slot=id==8 ? 0 : id==7 ? 31 : id==6 ? 100 : 3;
      BoxStorage::Snapshot snapshot;
      assert(snapshot.open({save.boxRoot.storeId,save.boxRoot.generation})==BoxStorage::Result::Ok);
      assert(snapshot.readSlot(slot,selected)==BoxStorage::Result::Ok); snapshot.close();
      const auto stats=calculateStats(selected);
      press(BUTTON_OK); assert(visible("능력치") && visible("기술") && visible("돌아가기"));
      assert(!visible("ID ") && graphics.text.find("ID ")==std::string::npos);
      press(BUTTON_OK); assert(visible("능력치 1/2"));
      char value[32]; std::snprintf(value,sizeof(value),"HP 3/%u",static_cast<unsigned>(stats.hp));
      assert(visible(value));
      std::snprintf(value,sizeof(value),"공격 %u",static_cast<unsigned>(stats.attack)); assert(visible(value));
      std::snprintf(value,sizeof(value),"방어 %u",static_cast<unsigned>(stats.defense)); assert(visible(value));
      assert(graphics.text.find("Lv.5 F")!=std::string::npos);
      assert((graphics.text.find("SHINY")!=std::string::npos)==selected.shiny);
      press(BUTTON_RIGHT); assert(visible("능력치 2/2"));
      std::snprintf(value,sizeof(value),"특공 %u",static_cast<unsigned>(stats.spAttack)); assert(visible(value));
      std::snprintf(value,sizeof(value),"특방 %u",static_cast<unsigned>(stats.spDefense)); assert(visible(value));
      std::snprintf(value,sizeof(value),"스피드 %u",static_cast<unsigned>(stats.speed)); assert(visible(value));
      press(BUTTON_LEFT); assert(visible("능력치 1/2"));
      press(BUTTON_BACK); assert(visible("돌아가기"));
      press(BUTTON_RIGHT); press(BUTTON_OK); assert(visible("기술 없음"));
      press(BUTTON_RIGHT); assert(visible("기술 1/1"));
      press(BUTTON_OK); press(BUTTON_OK); assert(visible("기술 없음"));
      press(BUTTON_BACK); press(BUTTON_BACK);
      if (row<3) press(BUTTON_RIGHT);
    }
    assert(visible("4/4"));
    press(BUTTON_RIGHT); assert(visible("돌아가기"));
    press(BUTTON_OK); assert(visible("최근 포획"));
    press(BUTTON_LEFT); press(BUTTON_OK); assert(!GameApp::boxBrowserActive());
  }
  search(); number("0025"); assert(visible("1/2") && visible("#025"));
  press(BUTTON_OK); assert(visible("능력치")); press(BUTTON_BACK); press(BUTTON_RIGHT); assert(visible("2/2"));
  assert(graphics.text.find("SHINY")!=std::string::npos);
  GameApp::leaveGameMode();
  search(); number("1025"); assert(visible("#1025") && visible("보유 없음"));
  press(BUTTON_OK); assert(visible("도감번호"));
  for (const char* invalid : {"0000","1026","9999"}) {
    search(); number(invalid); assert(visible("번호 범위") && visible("0001~1025"));
    press(BUTTON_OK); assert(visible("도감번호 찾기"));
    press(BUTTON_LEFT_LONG); assert(visible("도감번호") && !visible("도감번호 찾기"));
  }
  search(); press(BUTTON_OK); press(BUTTON_LEFT); // Digit 0 -> 9 wrap.
  bool nine=false;
  for (const auto& run : text.textRuns) if (run.x==16 && run.y==24 && run.value=="9") nine=true;
  assert(nine); press(BUTTON_RIGHT); press(BUTTON_LEFT_LONG); assert(visible("도감번호"));
  press(BUTTON_RIGHT); press(BUTTON_OK); assert(visible("1세대") && visible("2세대"));
  press(BUTTON_OK); assert(visible("1/4")); // Gen1 sorted by Dex.
  press(BUTTON_OK); assert(visible("구구"));
  search(); press(BUTTON_RIGHT); press(BUTTON_OK); press(BUTTON_RIGHT,8); press(BUTTON_OK);
  assert(visible("9세대") && visible("보유 없음"));
  press(BUTTON_OK); assert(visible("1세대")); // Return to generation menu.
  search(); press(BUTTON_RIGHT,2); press(BUTTON_OK);
  assert(visible("색이 다른") && visible("1/2")); press(BUTTON_OK); assert(visible("피카츄"));
  const auto reads=FakeLittleFS::readCalls;
  assert(!GameApp::saveState(GameApp::state()));
  GameApp::update(); assert(FakeLittleFS::readCalls==reads);
  GameApp::updatePokemonAnimation(MODE_DESK); // Actual mode exit cleanup.
  assert(!GameApp::boxBrowserActive() && FakeLittleFS::readOpens==FakeLittleFS::readCloses);
  for (unsigned visit=0;visit<100;++visit) {
    // Re-enter from the still-selected Home Box item without init/load hiding RAM changes.
    press(BUTTON_OK); assert(GameApp::boxBrowserActive());
    press(BUTTON_OK); press(BUTTON_RIGHT); press(BUTTON_OK);
    GameApp::leaveGameMode();
    assert(!GameApp::boxBrowserActive() && FakeLittleFS::readOpens==FakeLittleFS::readCloses);
  }
  assert(FakeNvs::data==original && browserFiles()==files);
  assert(FakeNvs::writes==writes && FakeLittleFS::writeCalls==fsWrites);
  auto ram=save; ram.state=GameApp::state();
  assert(browserSaveBytes(ram)==browserSaveBytes(save));
  enter(); press(BUTTON_OK); press(BUTTON_OK); press(BUTTON_RIGHT,2); press(BUTTON_OK);
  assert(visible("1/4")); press(BUTTON_BACK); assert(visible("최근 포획"));
  press(BUTTON_BACK); assert(!GameApp::boxBrowserActive());
  search(); press(BUTTON_OK); press(BUTTON_BACK); assert(visible("도감번호"));
  press(BUTTON_RIGHT); press(BUTTON_OK); press(BUTTON_BACK); assert(visible("도감번호"));
  press(BUTTON_BACK); assert(visible("최근 포획")); press(BUTTON_BACK);
  // Runtime failure after successful load/open, not a save-recovery fixture.
  enter(); press(BUTTON_OK); FakeLittleFS::readBudget=0; press(BUTTON_RIGHT);
  assert(visible("박스 읽기 오류") && visible("OK 돌아가기"));
  assert(FakeLittleFS::readOpens==FakeLittleFS::readCloses);
  FakeLittleFS::readBudget=std::numeric_limits<size_t>::max(); press(BUTTON_OK);
  assert(!GameApp::boxBrowserActive());
  for (unsigned failure=0;failure<4;++failure) {
    GameApp::init(); press(BUTTON_RIGHT,2);
    const auto path=browserPath(save.boxRoot);
    auto retained=FakeLittleFS::files.at(path);
    const auto originalBytes=*retained;
    if (failure==0) FakeLittleFS::files.erase(path);
    if (failure==1) retained->back()^=1;
    if (failure==2) FakeLittleFS::failOpen=true;
    if (failure==3) { // Valid snapshot with different CRC than the loaded root.
      (*retained)[BOX_RECORDS_OFFSET+6]^=1; browserFixCrc(*retained);
    }
    press(BUTTON_OK); assert(visible("박스 읽기 오류"));
    if (failure==0) FakeLittleFS::files[path]=retained;
    if (failure==1) retained->back()^=1;
    if (failure==3) *retained=originalBytes;
    FakeLittleFS::failOpen=false; press(BUTTON_OK);
  }
  assert(FakeNvs::data==original && browserFiles()==files);
  // Full Box navigation/header bounds; selecting and redrawing never rescans.
  installCaptureBoxFixture(save,BOX_CAPACITY,save.boxRoot.generation+1);
  enter(); press(BUTTON_OK); assert(visible("1/2048"));
  const auto beforeMove=FakeLittleFS::readCalls;
  press(BUTTON_RIGHT_LONG); assert(visible("11/2048"));
  assert(FakeLittleFS::readCalls==beforeMove+1);
  press(BUTTON_LEFT_LONG); assert(visible("1/2048"));
  press(BUTTON_LEFT); assert(visible("돌아가기"));
  press(BUTTON_LEFT); assert(visible("2048/2048"));
  press(BUTTON_OK); press(BUTTON_BACK); assert(visible("2048/2048"));
  press(BUTTON_RIGHT,2); assert(visible("1/2048"));
  search(); press(BUTTON_RIGHT,2); press(BUTTON_OK); assert(visible("색이 다른") && visible("보유 없음"));
  GameApp::leaveGameMode();
  assert(FakeLittleFS::readOpens==FakeLittleFS::readCloses);
  for (uint8_t count=0;count<=4;++count) {
    auto moveSave=browserFresh();
    auto pokemon=moveSave.state.party.members[0]; pokemon.instanceId=moveSave.state.progress.nextInstanceId++;
    for (uint8_t i=0;i<4;++i) pokemon.moves[i]=i<count ? ids[i] : 0;
    BoxKey key{moveSave.boxRoot.storeId,moveSave.boxRoot.generation};
    assert(BoxStorage::mutate(key,key.generation+1,{BoxMutationKind::Insert,0,pokemon})==BoxStorage::Result::Ok);
    ++key.generation; BoxMetadata metadata;
    assert(BoxStorage::validate(key,metadata)==BoxStorage::Result::Ok);
    moveSave.boxRoot=boxRootFromMetadata(metadata); assert(GameSaveStorage::save(moveSave));
    const auto nvsBefore=FakeNvs::data; const auto filesBefore=browserFiles();
    enter(); press(BUTTON_OK); press(BUTTON_OK); press(BUTTON_RIGHT); press(BUTTON_OK);
    if (!count) assert(visible("기술 없음"));
    for (uint8_t page=0;page<BoxDetails::movePages(pokemon);++page) {
      for (uint8_t i=0;i<count;++i) assert(visible(findMove(ids[i])->name)==(i/2==page));
      assert(!visible("PP"));
      press(BUTTON_RIGHT);
    }
    press(BUTTON_LEFT); press(BUTTON_BACK); assert(visible("돌아가기"));
    press(BUTTON_BACK); press(BUTTON_BACK); press(BUTTON_BACK);
    assert(!GameApp::boxBrowserActive());
    assert(nvsBefore==FakeNvs::data && filesBefore==browserFiles());
  }
  std::puts("PASS D.1 UI: stats/owned moves 0-4/unknown fallback/BACK hierarchy/no IDs; empty/menu/3 rows/details/sorts/number/generation/shiny/long wrap/100 visits/errors/mode close; read-only");
}
