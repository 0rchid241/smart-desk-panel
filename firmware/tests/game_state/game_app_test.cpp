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


bool graphicsVisible(
  const char* text
) {
  return
    deskOled.text.find(
      text
    ) !=
    std::string::npos;
}

} // namespace

void finishFeedback() {
  const auto writes = FakeNvs::writes;
  int count = 0;
  while (visible("데미지!") || visible("변화 기술") || visible("빗나갔다!")) {
    assert(++count <= 2);
    GameApp::handleButton(BUTTON_OK);
  }
  assert(FakeNvs::writes == writes);
}


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
  oled.textRuns.push_back({x, y, text});
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


void assertMoveViewport(const char* first, const char* second, int cursorRow) {
  bool firstFound = false, secondFound = second == nullptr;
  unsigned cursors = 0;
  for (const auto& run : gameOled.textRuns) {
    if (run.x == 12 && run.y == 18 && run.value == first) firstFound = true;
    if (second && run.x == 12 && run.y == 36 && run.value == second) secondFound = true;
    if (run.value == ">") {
      ++cursors;
      assert(run.x == 0 && run.y == 22 + cursorRow * 18);
    }
  }
  assert(firstFound && secondFound && cursors == 1);
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


  // G3-B: 그래픽 OLED도 WildEncounter 상태를 반영한다.
  // Host test에는 로컬 bitmap asset이 없으므로 fallback을 검증한다.
  GameApp::drawGameGraphics();


  assert(
    graphicsVisible(
      "WILD"
    )
  );

  assert(
    graphicsVisible(
      firstWild->name
    )
  );

  assert(
    graphicsVisible(
      "Lv."
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

  // G4: 기존 조우 소비 회귀는 이제 Battle 진입까지 검증한다.
  assert(GameApp::state().battle.status == BattleStatus::Active);
  assert(GameApp::state().battle.wild.speciesId == firstWildSpecies);
  assert(GameApp::state().battle.wild.gender == firstWildGender);
  assert(visible("기술 선택") && visible("전기쇼크") && visible("울음소리") && visible("PP 30/30"));
  assert(!visible("HP") && !visible("Lv.") && !visible("피카츄"));
  assertMoveViewport("전기쇼크", "울음소리", 0);
  GameApp::drawGameGraphics();
  assert(!graphicsVisible("YOU") && !graphicsVisible("WILD") && graphicsVisible(firstWild->name));
  assert(graphicsVisible("피카츄") && graphicsVisible("Lv.5") && graphicsVisible("HP 18/18"));
  GameApp::handleButton(BUTTON_RIGHT);
  assert(visible("울음소리") && visible("PP 40/40"));
  assertMoveViewport("전기쇼크", "울음소리", 1);
  GameApp::handleButton(BUTTON_LEFT);
  assert(visible("전기쇼크"));
  assertMoveViewport("전기쇼크", "울음소리", 0);

  auto stateBytes = [](const GameState& state) {
    GameSave save; save.state = state;
    std::vector<uint8_t> bytes(SAVE_MAX_SIZE); size_t size = 0;
    assert(serialize(save, bytes.data(), bytes.size(), size));
    bytes.resize(size); return bytes;
  };
  GameApp::handleButton(BUTTON_RIGHT); // 변화 기술로 첫 턴 이후 Active 보장.
  const auto beforeTurn = stateBytes(GameApp::state());
  auto expectedTurn = GameApp::state();
  assert(resolveBattleTurn(expectedTurn, 1));
  FakeNvs::partialWrite = true;
  GameApp::handleButton(BUTTON_OK);
  assert(stateBytes(GameApp::state()) == beforeTurn && visible("SAVE ERROR"));
  assert(!visible("변화 기술") && !visible("데미지!"));
  const auto failedTurnWrites = FakeNvs::writes;
  for (int i = 0; i < 100; ++i) GameApp::update();
  assert(FakeNvs::writes == failedTurnWrites);
  FakeNvs::partialWrite = false;
  GameApp::init(); // 실패한 턴은 재부팅해도 적용되지 않는다.
  assert(stateBytes(GameApp::state()) == beforeTurn);
  GameApp::handleButton(BUTTON_RIGHT);
  GameApp::handleButton(BUTTON_OK);
  assert(stateBytes(GameApp::state()) == stateBytes(expectedTurn));
  assert(GameApp::state().battle.turn == 1 && visible("피카츄의") && visible("울음소리!") && visible("변화 기술"));
  const auto afterTurn = stateBytes(GameApp::state());
  char hpText[32];
  snprintf(hpText, sizeof(hpText), "HP %u/18",
    static_cast<unsigned>(GameApp::state().battle.player.currentHp));
  GameApp::drawGameGraphics();
  assert(GameApp::state().battle.player.currentHp < 18 && graphicsVisible(hpText));
  GameApp::init(); GameApp::drawGameTextScreen();
  assert(stateBytes(GameApp::state()) == afterTurn && visible("PP 30/30"));
  assert(GameApp::state().battle.player.pp[1] == 39);
  // DESK 표시/복귀는 전투 진행이나 저장을 유발하지 않는다.
  const auto idleWrites = FakeNvs::writes;
  GameApp::drawDeskPet(); GameApp::updatePokemonAnimation(MODE_DESK);
  for (int i = 0; i < 100; ++i) GameApp::update();
  GameApp::drawGameGraphics(); GameApp::drawGameTextScreen();
  assert(stateBytes(GameApp::state()) == afterTurn && FakeNvs::writes == idleWrites);

  // 승리와 패배를 확정 상태로 만들되 실제 앱의 턴 입력을 거친다.
  auto victory = GameApp::state();
  victory.battle.opponent.currentHp = 1;
  assert(GameApp::saveState(victory));
  GameApp::handleButton(BUTTON_OK);
  finishFeedback();
  assert(GameApp::state().battle.status == BattleStatus::Won && visible("전투 승리!"));
  GameApp::init(); GameApp::drawGameTextScreen();
  assert(visible("전투 승리!"));
  const auto wonBytes = stateBytes(GameApp::state());
  FakeNvs::partialWrite = true; GameApp::handleButton(BUTTON_OK);
  assert(stateBytes(GameApp::state()) == wonBytes && visible("OK 재시도"));
  FakeNvs::partialWrite = false; GameApp::handleButton(BUTTON_OK);
  assert(GameApp::state().battle.status == BattleStatus::None);
  assert(partner(GameApp::state())->currentHp == calculateStats(*partner(GameApp::state())).hp);

  auto defeat = GameApp::state();
  assert(setEncounter(defeat.encounter, 25, 0, 100, Gender::Male, false));
  assert(startBattle(defeat)); defeat.battle.player.currentHp = 1;
  assert(GameApp::saveState(defeat));
  GameApp::init(); GameApp::handleButton(BUTTON_OK);
  finishFeedback();
  assert(GameApp::state().battle.status == BattleStatus::Lost && visible("쓰러졌다..."));
  GameApp::init(); GameApp::drawGameTextScreen();
  assert(visible("쓰러졌다..."));
  const auto lostBytes = stateBytes(GameApp::state());
  FakeNvs::partialWrite = true; GameApp::handleButton(BUTTON_OK);
  assert(stateBytes(GameApp::state()) == lostBytes);
  FakeNvs::partialWrite = false; GameApp::handleButton(BUTTON_OK);
  assert(GameApp::state().battle.status == BattleStatus::None);
  assert(partner(GameApp::state())->currentHp == 18);
  auto exhausted = GameApp::state();
  assert(setEncounter(exhausted.encounter,19,0,2,Gender::Male,false));
  assert(startBattle(exhausted));
  for (auto& pp : exhausted.battle.player.pp) pp = 0;
  for (auto& pp : exhausted.battle.opponent.pp) pp = 0;
  assert(GameApp::saveState(exhausted)); GameApp::init(); GameApp::drawGameTextScreen();
  assert(visible("발버둥") && visible("PP --"));
  assertMoveViewport("발버둥", nullptr, 0);
  GameApp::handleButton(BUTTON_LEFT); GameApp::handleButton(BUTTON_RIGHT);
  assert(visible("발버둥"));
  for (int i = 0; i < 30 && GameApp::state().battle.status == BattleStatus::Active; ++i)
    GameApp::handleButton(BUTTON_OK);
  assert(GameApp::state().battle.status != BattleStatus::Active);
  finishFeedback();
  GameApp::handleButton(BUTTON_OK);
  assert(GameApp::state().battle.status == BattleStatus::None && partner(GameApp::state())->currentHp == 18);
  std::puts("PASS app battle: start/turn/result atomicity, HP/PP UI, navigation, reboot, DESK resume, Won/Lost");

  auto fourMoves = createNewGame();
  auto& four = fourMoves.party.members[0];
  four.moves[0] = 21; four.moves[1] = 33; four.moves[2] = 45; four.moves[3] = 84;
  assert(setEncounter(fourMoves.encounter,19,0,2,Gender::Male,false));
  assert(startBattle(fourMoves)); fourMoves.battle.rngState = 1;
  assert(GameApp::saveState(fourMoves)); GameApp::init(); GameApp::drawGameTextScreen();
  assert(visible("힘껏치기") && visible("몸통박치기") && visible("PP 20/20"));
  assertMoveViewport("힘껏치기", "몸통박치기", 0);
  GameApp::handleButton(BUTTON_LEFT); assert(visible("PP 30/30"));
  assertMoveViewport("울음소리", "전기쇼크", 1);
  GameApp::handleButton(BUTTON_RIGHT); assert(visible("PP 20/20"));
  assertMoveViewport("힘껏치기", "몸통박치기", 0);
  GameApp::handleButton(BUTTON_RIGHT); assert(visible("PP 35/35"));
  assertMoveViewport("힘껏치기", "몸통박치기", 1);
  GameApp::handleButton(BUTTON_RIGHT); assert(visible("PP 40/40"));
  assertMoveViewport("몸통박치기", "울음소리", 1);
  GameApp::handleButton(BUTTON_RIGHT); assert(visible("PP 30/30"));
  assertMoveViewport("울음소리", "전기쇼크", 1);
  GameApp::handleButton(BUTTON_LEFT); assertMoveViewport("울음소리", "전기쇼크", 0);
  GameApp::handleButton(BUTTON_LEFT); assertMoveViewport("몸통박치기", "울음소리", 0);
  GameApp::handleButton(BUTTON_RIGHT);
  GameApp::handleButton(BUTTON_RIGHT);
  GameApp::handleButton(BUTTON_RIGHT); assert(visible("PP 20/20"));
  GameApp::handleButton(BUTTON_OK);
  assert(visible("힘껏치기!") && visible("빗나갔다!"));
  const auto messageWrites = FakeNvs::writes;
  GameApp::handleButton(BUTTON_LEFT); assert(visible("빗나갔다!"));
  GameApp::handleButton(BUTTON_OK);
  assert(visible("꼬렛의") && visible("몸통박치기!") && visible("데미지!"));
  GameApp::handleButton(BUTTON_OK);
  assert(visible("기술 선택") && FakeNvs::writes == messageWrites);
  auto skip = GameApp::state(); skip.battle.player.pp[0] = 0; skip.battle.player.pp[2] = 0;
  assert(GameApp::saveState(skip)); GameApp::init(); GameApp::drawGameTextScreen();
  assert(visible("PP 35/35"));
  assertMoveViewport("힘껏치기", "몸통박치기", 1);
  GameApp::handleButton(BUTTON_RIGHT); assert(visible("PP 30/30"));
  assertMoveViewport("울음소리", "전기쇼크", 1);
  GameApp::handleButton(BUTTON_RIGHT); assert(visible("PP 35/35"));
  assertMoveViewport("몸통박치기", "울음소리", 0);
  for (int i = 0; i < 30 && GameApp::state().battle.status == BattleStatus::Active; ++i) {
    GameApp::handleButton(BUTTON_OK); finishFeedback();
  }
  assert(GameApp::state().battle.status != BattleStatus::Active);
  GameApp::handleButton(BUTTON_OK);
  assert(GameApp::state().battle.status == BattleStatus::None);

  assert(
    visible(
      "피카츄"
    )
  );


  GameApp::drawGameGraphics();

  assert(
    graphicsVisible(
      "포켓몬"
    )
  );

  assert(
    !graphicsVisible(
      "WILD"
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
    "PASS app: persisted encounter, wild graphics fallback, reboot stability, atomic acknowledge"
  );
}
