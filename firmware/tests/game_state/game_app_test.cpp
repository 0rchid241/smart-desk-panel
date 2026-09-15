#include "game_app.h"
#include "save_storage.h"
#include "Preferences.h"
#include <Adafruit_SSD1306.h>
#include <cassert>
#include <cstdio>

namespace {
Adafruit_SSD1306 deskOled, gameOled;
uint64_t epoch = 0;
bool visible(const char* text) { return gameOled.text.find(text) != std::string::npos; }
}
namespace Displays {
Adafruit_SSD1306& desk() { return deskOled; }
Adafruit_SSD1306& game() { return gameOled; }
}
namespace NetworkTime {
bool getCurrentEpoch(uint64_t& result) { result = epoch; return epoch >= 1700000000ULL; }
}
void drawUtf8Text(Adafruit_SSD1306& oled, int16_t x, int16_t y, const char* text) {
  assert(x >= 0 && x < 128 && y >= 0 && y + 16 <= 64);
  int width = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
    if (*p < 128) width += 6;
    else if ((*p & 0xc0) == 0xc0) width += 16; // Current fixture uses Korean/ASCII only.
  }
  assert(x + width <= 128);
  oled.text += text;
}
void drawScrollingUtf8Text(Adafruit_SSD1306& oled, int16_t x, int16_t y, int16_t width, const char* text) {
  assert(width <= 128);
  drawUtf8Text(oled, x, y, text);
}

int main() {
  using namespace PokemonGame;
  FakeNvs::reset();
  GameApp::init();
  GameApp::drawGameTextScreen();
  assert(visible("피카츄"));
  GameApp::handleButton(BUTTON_OK);
  assert(visible("친밀도") && visible("EXP"));
  GameApp::handleButton(BUTTON_OK); // Status -> Home.
  GameApp::handleButton(BUTTON_RIGHT); // Explore.
  GameApp::handleButton(BUTTON_OK);
  assert(visible("지역 선택") && visible("테스트 초원"));
  const unsigned initialWrites = FakeNvs::writes;
  GameApp::handleButton(BUTTON_OK); // Invalid wall clock: do not save/start.
  assert(visible("시간 동기화 중...") && FakeNvs::writes == initialWrites);
  assert(GameApp::state().exploration.status == ExplorationStatus::Idle);
  GameApp::handleButton(BUTTON_RIGHT);
  assert(visible("돌아가기"));
  GameApp::handleButton(BUTTON_OK);
  assert(visible("피카츄"));
  GameApp::handleButton(BUTTON_OK); // Explore selection remains selected.
  epoch = 1800000000ULL;
  FakeNvs::partialWrite = true;
  GameApp::handleButton(BUTTON_OK);
  assert(visible("저장 오류") && GameApp::state().exploration.status == ExplorationStatus::Idle);
  FakeNvs::partialWrite = false;
  GameApp::handleButton(BUTTON_OK);
  assert(visible("탐험 중...") && visible("남은 15초"));
  assert(GameApp::state().exploration.startedAtEpoch == epoch);
  const auto startBytes = FakeNvs::data;
  const unsigned startWrites = FakeNvs::writes;
  for (unsigned i = 0; i < 100; ++i) GameApp::update();
  assert(FakeNvs::writes == startWrites);

  // Offline reboot preserves the pending session without drawing completion.
  epoch = 0;
  GameApp::init();
  GameApp::drawGameTextScreen();
  assert(visible("시간 확인 중..."));
  assert(GameApp::state().exploration.startedAtEpoch == 1800000000ULL);
  GameApp::update();
  assert(FakeNvs::data == startBytes && FakeNvs::writes == startWrites);
  epoch = 1799999999ULL; // Backward clock: wait, no underflow/completion.
  GameApp::update(); GameApp::drawGameTextScreen();
  assert(visible("시간 확인 중...") && FakeNvs::writes == startWrites);
  epoch = 1800000014ULL;
  GameApp::update(); GameApp::drawGameTextScreen();
  assert(visible("남은 1초") && FakeNvs::writes == startWrites);

  // Update is mode-independent and does not paint either OLED (Desk/notification safe).
  const unsigned draws = gameOled.draws + deskOled.draws;
  epoch = 1800000030ULL;
  FakeNvs::partialWrite = true;
  GameApp::update();
  assert(gameOled.draws + deskOled.draws == draws);
  assert(GameApp::state().exploration.status == ExplorationStatus::Exploring);
  const unsigned failedWrites = FakeNvs::writes;
  for (unsigned i = 0; i < 100; ++i) GameApp::update();
  assert(FakeNvs::writes == failedWrites); // No per-loop flash retry.
  GameApp::drawGameTextScreen();
  assert(visible("저장 오류") && visible("OK 재시도"));
  FakeNvs::partialWrite = false;
  GameApp::handleButton(BUTTON_OK);
  assert(GameApp::state().exploration.status == ExplorationStatus::Complete && visible("탐험 완료!"));
  const unsigned completeWrites = FakeNvs::writes;
  for (unsigned i = 0; i < 100; ++i) GameApp::update();
  assert(FakeNvs::writes == completeWrites);
  epoch = 0; GameApp::init(); GameApp::drawGameTextScreen();
  assert(visible("탐험 완료!")); // Complete survives boot even without time.
  FakeNvs::partialWrite = true;
  GameApp::handleButton(BUTTON_OK);
  assert(visible("저장 오류") && GameApp::state().exploration.status == ExplorationStatus::Complete);
  FakeNvs::partialWrite = false;
  GameApp::handleButton(BUTTON_OK);
  assert(GameApp::state().exploration.status == ExplorationStatus::Idle && visible("피카츄"));
  const unsigned finalWrites = FakeNvs::writes;
  GameApp::init(); GameApp::update(); GameApp::drawGameTextScreen();
  assert(visible("피카츄") && FakeNvs::writes == finalWrites);
  assert(partner(GameApp::state())->exp == 0 && partner(GameApp::state())->friendship == 70);
  std::puts("PASS app: Korean flow, invalid clock, offline reboot, background update, save transitions/retries");
}
