#pragma once
#include "browser_test_support.h"
#include "game_app.h"
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
  GameApp::leaveGameMode();
  auto save=browserFresh();
  const auto emptyNvs=FakeNvs::data; const auto emptyFiles=browserFiles();
  enter(); assert(visible("박스가 비어 있다"));
  press(BUTTON_RIGHT_LONG); assert(visible("박스가 비어 있다"));
  press(BUTTON_OK); assert(!GameApp::boxBrowserActive());
  assert(emptyNvs==FakeNvs::data && emptyFiles==browserFiles());

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
    press(BUTTON_OK); // Detail, same cached selected record.
    for (unsigned row=0;row<4;++row) {
      char id[24]; std::snprintf(id,sizeof(id),"ID %lu",static_cast<unsigned long>(expected[view][row]));
      assert(visible(id) && visible("HP 3/"));
      if (row<3) press(BUTTON_RIGHT);
    }
    press(BUTTON_RIGHT_LONG); // (3+10)%4 == 1
    char id[24]; std::snprintf(id,sizeof(id),"ID %lu",static_cast<unsigned long>(expected[view][1]));
    assert(visible(id)); press(BUTTON_LEFT_LONG); // returns row 3
    press(BUTTON_OK); assert(visible("4/4"));
    press(BUTTON_RIGHT); assert(visible("돌아가기"));
    press(BUTTON_OK); assert(visible("최근 포획"));
    press(BUTTON_LEFT); press(BUTTON_OK); assert(!GameApp::boxBrowserActive());
  }
  search(); number("0025"); assert(visible("1/2") && visible("#025"));
  press(BUTTON_OK); assert(visible("ID 8")); press(BUTTON_RIGHT); assert(visible("ID 7"));
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
  press(BUTTON_OK); assert(visible("ID 6"));
  search(); press(BUTTON_RIGHT); press(BUTTON_OK); press(BUTTON_RIGHT,8); press(BUTTON_OK);
  assert(visible("9세대") && visible("보유 없음"));
  press(BUTTON_OK); assert(visible("1세대")); // Return to generation menu.
  search(); press(BUTTON_RIGHT,2); press(BUTTON_OK);
  assert(visible("색이 다른") && visible("1/2")); press(BUTTON_OK); assert(visible("ID 7"));
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
  press(BUTTON_OK); press(BUTTON_RIGHT); // Detail wraps directly to Pokemon 1.
  press(BUTTON_OK); assert(visible("1/2048"));
  search(); press(BUTTON_RIGHT,2); press(BUTTON_OK); assert(visible("색이 다른") && visible("보유 없음"));
  GameApp::leaveGameMode();
  assert(FakeLittleFS::readOpens==FakeLittleFS::readCloses);
  std::puts("PASS D UI: empty/menu/3 rows/details/sorts/number/generation/shiny/long wrap/100 visits/errors/mode close; read-only");
}
