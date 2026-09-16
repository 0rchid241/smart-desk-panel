#include "game_app.h"
#include "save_storage.h"
#include "Preferences.h"
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <cassert>
#include <cstdio>

namespace { Adafruit_SSD1306 desk, game; }
namespace Displays {
Adafruit_SSD1306& desk() { return ::desk; }
Adafruit_SSD1306& game() { return ::game; }
}
namespace NetworkTime { bool getCurrentEpoch(uint64_t& epoch) { epoch = 0; return false; } }
void drawUtf8Text(Adafruit_SSD1306& oled, int16_t x, int16_t y, const char* text) {
  int width = 0;
  for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
    width += *p < 128 ? 6 : ((*p & 0xc0) == 0xc0 ? 16 : 0);
  assert(x >= 0 && x + width <= 128 && y >= 0 && y + 16 <= 64);
  oled.text += text;
}
void drawScrollingUtf8Text(Adafruit_SSD1306& oled, int16_t x, int16_t y, int16_t, const char* text) {
  drawUtf8Text(oled,x,y,text);
}
int main() {
  using namespace PokemonGame;
  FakeNvs::reset(); GameApp::init();
  assert(game.bitmaps > 0);
  for (SpeciesId id : {SpeciesId(16),SpeciesId(19),SpeciesId(25)}) {
    auto next = createNewGame();
    assert(setEncounter(next.encounter,id,0,100,Gender::Female,true));
    assert(GameApp::saveState(next)); GameApp::init();
    const auto oldBitmaps = desk.bitmaps;
    GameApp::drawGameGraphics(); assert(desk.bitmaps == oldBitmaps + 1);
    // 조우와 전투 상태 모두 DESK 펫 애니메이션이 계속 움직인다.
    auto checkDesk = []() {
      const auto old = game.bitmaps;
      hostMillis += 2100;
      GameApp::updatePokemonAnimation(MODE_DESK);
      assert(game.bitmaps == old + 1);
    };
    checkDesk();
    GameApp::handleButton(BUTTON_OK);
    assert(GameApp::state().battle.status == BattleStatus::Active);
    const auto pixels = desk.pixels;
    GameApp::drawGameGraphics(); assert(desk.pixels > pixels);
    assert(desk.text.find("WILD") != std::string::npos && desk.text.find("YOU") != std::string::npos);
    GameApp::drawGameTextScreen(); checkDesk();
    const auto writes = FakeNvs::writes, draws = desk.draws;
    hostMillis += 2100; GameApp::updatePokemonAnimation(MODE_GAME); GameApp::update();
    assert(desk.draws == draws && FakeNvs::writes == writes);
    assert(GameApp::state().battle.turn == 0);
  }
  std::puts("PASS asset app: synthetic sprites, pixel/text bounds, GAME static battle, DESK animation while encounter/battle");
}
