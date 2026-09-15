#include "game_app.h"
#include "save_storage.h"
#include "Preferences.h"

#include <Adafruit_SSD1306.h>

#include <cassert>
#include <cstdio>


namespace {

Adafruit_SSD1306 deskOled;
Adafruit_SSD1306 gameOled;

uint64_t epoch =
  0;


bool visible(
  const char* text
) {
  return
    gameOled.text.find(
      text
    ) !=
    std::string::npos;
}

} // namespace


namespace Displays {

Adafruit_SSD1306& desk() {
  return deskOled;
}

Adafruit_SSD1306& game() {
  return gameOled;
}

} // namespace Displays


namespace NetworkTime {

bool getCurrentEpoch(
  uint64_t& result
) {
  result =
    epoch;

  return
    epoch >=
    1700000000ULL;
}

} // namespace NetworkTime


void drawUtf8Text(
  Adafruit_SSD1306& oled,
  int16_t x,
  int16_t y,
  const char* text
) {
  assert(
    x >= 0 &&
    x < 128 &&
    y >= 0 &&
    y + 16 <= 64
  );


  int width =
    0;


  for (
    const unsigned char* p =
      reinterpret_cast<const unsigned char*>(
        text
      );
    *p;
    ++p
  ) {

    if (
      *p < 128
    ) {

      width +=
        6;

    } else if (
      (*p & 0xc0) ==
      0xc0
    ) {

      width +=
        16;
    }
  }


  assert(
    x + width <=
    128
  );


  oled.text +=
    text;
}


void drawScrollingUtf8Text(
  Adafruit_SSD1306& oled,
  int16_t x,
  int16_t y,
  int16_t width,
  const char* text
) {
  assert(
    width <=
    128
  );


  drawUtf8Text(
    oled,
    x,
    y,
    text
  );
}


int main() {
  using namespace PokemonGame;


  FakeNvs::reset();


  GameApp::init();

  GameApp::drawGameTextScreen();


  assert(
    visible(
      "피카츄"
    )
  );


  // 상태 화면.
  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    visible(
      "친밀도"
    ) &&
    visible(
      "EXP"
    )
  );


  GameApp::handleButton(
    BUTTON_OK
  );  // Status -> Home.


  // 탐험 메뉴.
  GameApp::handleButton(
    BUTTON_RIGHT
  );

  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    visible(
      "지역 선택"
    ) &&
    visible(
      "테스트 초원"
    )
  );


  const unsigned initialWrites =
    FakeNvs::writes;


  // 잘못된 wall clock에서는 탐험 시작/저장 금지.
  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    visible(
      "시간 동기화 중..."
    ) &&
    FakeNvs::writes ==
      initialWrites
  );

  assert(
    GameApp::state().exploration.status ==
    ExplorationStatus::Idle
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::None
  );


  // 돌아가기 확인.
  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "돌아가기"
    )
  );


  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    visible(
      "피카츄"
    )
  );


  // 탐험 메뉴가 선택된 상태이므로 다시 진입.
  GameApp::handleButton(
    BUTTON_OK
  );


  epoch =
    1800000000ULL;


  // 시작 save 실패.
  FakeNvs::partialWrite =
    true;


  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    visible(
      "저장 오류"
    ) &&
    GameApp::state().exploration.status ==
      ExplorationStatus::Idle
  );


  FakeNvs::partialWrite =
    false;


  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    visible(
      "탐험 중..."
    ) &&
    visible(
      "남은 15초"
    )
  );


  assert(
    GameApp::state().exploration.startedAtEpoch ==
    epoch
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::None
  );


  const auto startBytes =
    FakeNvs::data;

  const unsigned startWrites =
    FakeNvs::writes;


  // 탐험이 끝나지 않았으면 update는 flash write를 하지 않는다.
  for (
    unsigned i = 0;
    i < 100;
    ++i
  ) {
    GameApp::update();
  }


  assert(
    FakeNvs::writes ==
    startWrites
  );


  // Offline reboot:
  // 진행 중인 세션을 그대로 복원하고 완료 처리하지 않는다.
  epoch =
    0;


  GameApp::init();

  GameApp::drawGameTextScreen();


  assert(
    visible(
      "시간 확인 중..."
    )
  );

  assert(
    GameApp::state().exploration.startedAtEpoch ==
    1800000000ULL
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::None
  );


  GameApp::update();


  assert(
    FakeNvs::data ==
      startBytes &&
    FakeNvs::writes ==
      startWrites
  );


  // 시간이 뒤로 간 경우도 대기.
  epoch =
    1799999999ULL;


  GameApp::update();

  GameApp::drawGameTextScreen();


  assert(
    visible(
      "시간 확인 중..."
    ) &&
    FakeNvs::writes ==
      startWrites
  );


  epoch =
    1800000014ULL;


  GameApp::update();

  GameApp::drawGameTextScreen();


  assert(
    visible(
      "남은 1초"
    ) &&
    FakeNvs::writes ==
      startWrites
  );


  // update는 device mode와 독립적이고 OLED를 직접 그리지 않는다.
  const unsigned draws =
    gameOled.draws +
    deskOled.draws;


  epoch =
    1800000030ULL;


  // 완료 + encounter 저장 실패.
  FakeNvs::partialWrite =
    true;


  GameApp::update();


  assert(
    gameOled.draws +
      deskOled.draws ==
    draws
  );

  assert(
    GameApp::state().exploration.status ==
    ExplorationStatus::Exploring
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::None
  );


  const unsigned failedWrites =
    FakeNvs::writes;


  // loop마다 flash 재시도하지 않는다.
  for (
    unsigned i = 0;
    i < 100;
    ++i
  ) {
    GameApp::update();
  }


  assert(
    FakeNvs::writes ==
    failedWrites
  );


  GameApp::drawGameTextScreen();


  assert(
    visible(
      "저장 오류"
    ) &&
    visible(
      "OK 재시도"
    )
  );


  // 사용자가 OK를 누르면 같은 탐험 정보로 동일 encounter를 다시 계산한다.
  FakeNvs::partialWrite =
    false;


  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    GameApp::state().exploration.status ==
    ExplorationStatus::Complete
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::Ready
  );


  const SpeciesId firstWildSpecies =
    GameApp::state().encounter.speciesId;

  const FormId firstWildForm =
    GameApp::state().encounter.formId;

  const uint8_t firstWildLevel =
    GameApp::state().encounter.level;

  const Gender firstWildGender =
    GameApp::state().encounter.gender;


  const auto* firstWild =
    findSpecies(
      firstWildSpecies,
      firstWildForm
    );


  assert(
    firstWild !=
    nullptr
  );


  assert(
    visible(
      "야생의"
    )
  );

  assert(
    visible(
      firstWild->name
    )
  );


  const unsigned completeWrites =
    FakeNvs::writes;


  // 완료 후 update 반복으로 재추첨/재저장되지 않는다.
  for (
    unsigned i = 0;
    i < 100;
    ++i
  ) {
    GameApp::update();
  }


  assert(
    FakeNvs::writes ==
    completeWrites
  );

  assert(
    GameApp::state().encounter.speciesId ==
    firstWildSpecies
  );


  // 전원을 껐다 켠 상황:
  // 시간 정보가 없어도 저장된 동일 encounter를 그대로 보여준다.
  epoch =
    0;


  GameApp::init();

  GameApp::drawGameTextScreen();


  assert(
    visible(
      "야생의"
    )
  );

  assert(
    GameApp::state().exploration.status ==
    ExplorationStatus::Complete
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::Ready
  );

  assert(
    GameApp::state().encounter.speciesId ==
    firstWildSpecies
  );

  assert(
    GameApp::state().encounter.formId ==
    firstWildForm
  );

  assert(
    GameApp::state().encounter.level ==
    firstWildLevel
  );

  assert(
    GameApp::state().encounter.gender ==
    firstWildGender
  );


  // 조우 확인 저장 실패:
  // exploration + encounter 모두 그대로 남아야 한다.
  FakeNvs::partialWrite =
    true;


  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    visible(
      "OK 재시도"
    )
  );

  assert(
    GameApp::state().exploration.status ==
    ExplorationStatus::Complete
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::Ready
  );


  // 다시 OK → exploration Idle + encounter None을 한 번에 저장.
  FakeNvs::partialWrite =
    false;


  GameApp::handleButton(
    BUTTON_OK
  );


  assert(
    GameApp::state().exploration.status ==
    ExplorationStatus::Idle
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::None
  );

  assert(
    visible(
      "피카츄"
    )
  );


  const unsigned finalWrites =
    FakeNvs::writes;


  // 최종 reboot에서도 홈 유지.
  GameApp::init();

  GameApp::update();

  GameApp::drawGameTextScreen();


  assert(
    visible(
      "피카츄"
    ) &&
    FakeNvs::writes ==
      finalWrites
  );


  assert(
    partner(
      GameApp::state()
    )->exp ==
      0
  );

  assert(
    partner(
      GameApp::state()
    )->friendship ==
      70
  );


  std::puts(
    "PASS app: exploration -> persisted encounter, reboot stability, atomic acknowledge, save retries"
  );
}
