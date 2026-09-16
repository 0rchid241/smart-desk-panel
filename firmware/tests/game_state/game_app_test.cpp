#include "capture_test_support.h"
#include "game_app.h"
#include "save_storage.h"
#include "Preferences.h"

#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include <cassert>
#include <cstdio>
#include <vector>

namespace {

Adafruit_SSD1306 deskOled;
Adafruit_SSD1306 gameOled;

uint64_t epoch = 0;

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
  result = epoch;

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

  int width = 0;

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
      width += 6;
    } else if (
      (*p & 0xc0) ==
      0xc0
    ) {
      width += 16;
    }
  }

  assert(
    x + width <=
    128
  );

  oled.text += text;
  oled.textRuns.push_back(
    {
      x,
      y,
      text
    }
  );
}

void drawScrollingUtf8Text(
  Adafruit_SSD1306& oled,
  int16_t x,
  int16_t y,
  int16_t width,
  const char* text
) {
  assert(
    width <= 128
  );

  drawUtf8Text(
    oled,
    x,
    y,
    text
  );
}

void assertTwoRowViewport(
  const char* first,
  const char* second,
  int cursorRow
) {
  bool firstFound = false;
  bool secondFound =
    second == nullptr;
  unsigned cursors = 0;

  for (
    const auto& run :
    gameOled.textRuns
  ) {
    if (
      run.x == 12 &&
      run.y == 18 &&
      run.value == first
    ) {
      firstFound = true;
    }

    if (
      second &&
      run.x == 12 &&
      run.y == 36 &&
      run.value == second
    ) {
      secondFound = true;
    }

    if (
      run.value == ">"
    ) {
      ++cursors;

      assert(
        run.x == 0 &&
        run.y ==
          22 +
          cursorRow * 18
      );
    }
  }

  assert(
    firstFound &&
    secondFound &&
    cursors == 1
  );
}

void assertMoveViewport(
  const char* first,
  const char* second,
  int cursorRow
) {
  assertTwoRowViewport(
    first,
    second,
    cursorRow
  );
}

void assertCommandViewport(
  const char* first,
  const char* second,
  int cursorRow
) {
  assertTwoRowViewport(
    first,
    second,
    cursorRow
  );
}

void assertCommandScreen() {
  assert(
    visible(
      "행동 선택"
    )
  );

  assert(
    !visible(
      "기술 선택"
    )
  );
}

void enterFight() {
  assertCommandScreen();

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "기술 선택"
    )
  );
}

void enterBag() {
  assertCommandScreen();

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "볼 선택"
    )
  );
}

void finishFeedback() {
  const auto writes =
    FakeNvs::writes;

  int count = 0;

  while (
    visible(
      "데미지!"
    ) ||
    visible(
      "변화 기술"
    ) ||
    visible(
      "빗나갔다!"
    )
  ) {
    assert(
      ++count <= 2
    );

    GameApp::handleButton(
      BUTTON_OK
    );
  }

  assert(
    FakeNvs::writes ==
    writes
  );
}

void finishCaptureAnimation(
  const char* expectedResult
) {
  assert(
    visible(
      "포획 중..."
    )
  );

  const unsigned writes =
    FakeNvs::writes;

  const unsigned draws =
    deskOled.draws;

  // 성공 3회 흔들림도 충분히 끝나는 시간까지 GAME loop를 진행한다.
  int steps = 0;

  while (
    !visible(
      expectedResult
    ) &&
    steps++ <
      24
  ) {
    hostMillis +=
      150;

    GameApp::updatePokemonAnimation(
      MODE_GAME
    );
  }

  assert(
    visible(
      expectedResult
    )
  );

  assert(
    FakeNvs::writes ==
    writes
  );

  assert(
    deskOled.draws >
    draws
  );
}

int main() {
  using namespace PokemonGame;

  auto stateBytes =
    [](
      const GameState& state
    ) {
      GameSave save; save.boxRoot = wireTestRoot();
      save.state =
        state;

      std::vector<uint8_t> bytes(
        SAVE_MAX_SIZE
      );

      size_t size = 0;

      assert(
        serialize(
          save,
          bytes.data(),
          bytes.size(),
          size
        )
      );

      bytes.resize(
        size
      );

      return bytes;
    };

  resetStorageFakes();

  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    visible(
      "피카츄"
    )
  );

  // 상태 화면 회귀.
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
  );

  // C1.1: 홈에서 읽기 전용 파티 조회. 새 게임은 피카츄 1마리다.
  const unsigned partyViewWrites =
    FakeNvs::writes;

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "파티"
    )
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "파티 1/1"
    ) &&
    visible(
      "피카츄"
    ) &&
    visible(
      "Lv.5"
    )
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "파티 1/1"
    ) &&
    FakeNvs::writes ==
      partyViewWrites
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  // 탐험 메뉴는 파티 다음 항목이다.
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

  // 돌아가기 후 다시 탐험으로 진입.
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

  GameApp::handleButton(
    BUTTON_OK
  );

  epoch =
    1800000000ULL;

  // 시작 save 실패는 RAM 상태를 바꾸지 않는다.
  FakeNvs::partialWrite = true;

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

  FakeNvs::partialWrite = false;

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

  const auto startBytes =
    FakeNvs::data;

  const unsigned startWrites =
    FakeNvs::writes;

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

  // Offline reboot: 같은 탐험을 복원한다.
  epoch = 0;

  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    visible(
      "시간 확인 중..."
    )
  );

  assert(
    FakeNvs::data ==
    startBytes
  );

  epoch =
    1800000030ULL;

  // 완료 + 조우 save 실패.
  FakeNvs::partialWrite = true;

  GameApp::update();

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

  FakeNvs::partialWrite = false;

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
    firstWild != nullptr
  );

  assert(
    visible(
      "야생의"
    ) &&
    visible(
      firstWild->name
    )
  );

  GameApp::drawGameGraphics();

  assert(
    graphicsVisible(
      "WILD"
    ) &&
    graphicsVisible(
      firstWild->name
    ) &&
    graphicsVisible(
      "Lv."
    )
  );

  // 조우는 reboot에도 동일하다.
  epoch = 0;

  GameApp::init();
  GameApp::drawGameTextScreen();

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

  // Battle 시작 save 실패는 encounter를 보존한다.
  FakeNvs::partialWrite = true;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "OK 재시도"
    )
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::Ready
  );

  FakeNvs::partialWrite = false;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    GameApp::state().battle.status ==
    BattleStatus::Active
  );

  assert(
    GameApp::state().encounter.status ==
    EncounterStatus::None
  );

  // G5-A1: 전투 시작점은 기술 목록이 아니라 명령 메뉴다.
  assertCommandScreen();
  assert(
    visible(
      "싸운다"
    ) &&
    visible(
      "가방"
    )
  );

  assertCommandViewport(
    "싸운다",
    "가방",
    0
  );

  const auto commandState =
    stateBytes(
      GameApp::state()
    );

  const unsigned commandWrites =
    FakeNvs::writes;

  // 명령 메뉴 이동은 GameState/NVS를 건드리지 않는다.
  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assertCommandViewport(
    "싸운다",
    "가방",
    1
  );

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      commandState &&
    FakeNvs::writes ==
      commandWrites
  );

  // G5-B: 가방은 해금된 볼 목록과 돌아가기를 보여준다.
  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "볼 선택"
    ) &&
    visible(
      "몬스터볼"
    ) &&
    visible(
      "돌아가기"
    )
  );

  assertTwoRowViewport(
    "몬스터볼",
    "돌아가기",
    0
  );

  // 기본 ballTier 0에서는 몬스터볼 하나만 있고, 돌아가기로 전투 명령에 복귀한다.
  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assertTwoRowViewport(
    "몬스터볼",
    "돌아가기",
    1
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assertCommandViewport(
    "싸운다",
    "가방",
    0
  );

  // 포켓몬 placeholder.
  GameApp::handleButton(
    BUTTON_RIGHT
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assertCommandViewport(
    "가방",
    "포켓몬",
    1
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "포켓몬"
    ) &&
    visible(
      "아직 준비 중"
    )
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  // G5-A2: 도망 save 실패는 HP/PP/RNG/turn을 전혀 적용하지 않는다.
  auto runSeeded =
    GameApp::state();

  runSeeded.battle.rngState =
    4; // fixture 3종 모두 첫 roll=76이라 현재 단순 도주식에서 실패한다.

  assert(
    GameApp::saveState(
      runSeeded
    )
  );

  GameApp::init();
  GameApp::drawGameTextScreen();

  assertCommandScreen();

  GameApp::handleButton(
    BUTTON_LEFT
  );

  assertCommandViewport(
    "포켓몬",
    "도망",
    1
  );

  const auto beforeRun =
    stateBytes(
      GameApp::state()
    );

  const unsigned beforeRunWrites =
    FakeNvs::writes;

  FakeNvs::partialWrite =
    true;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      beforeRun
  );

  assert(
    GameApp::state().battle.status ==
      BattleStatus::Active
  );

  assert(
    visible(
      "SAVE ERROR"
    )
  );

  assert(
    !visible(
      "도망 실패!"
    )
  );

  FakeNvs::partialWrite =
    false;

  // 저장 실패 후 재부팅해도 도망 시도 자체가 없었던 상태로 복원된다.
  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      beforeRun
  );

  assertCommandScreen();

  // 같은 seed로 재시도하면 같은 도망 실패 결과가 저장된다.
  GameApp::handleButton(
    BUTTON_LEFT
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "도망 실패!"
    ) &&
    visible(
      "상대가 공격한다"
    )
  );

  assert(
    GameApp::state().battle.status ==
      BattleStatus::Active
  );

  assert(
    GameApp::state().battle.turn ==
      1
  );

  assert(
    stateBytes(
      GameApp::state()
    ) !=
      beforeRun
  );

  assert(
    FakeNvs::writes >
      beforeRunWrites
  );

  const auto afterFailedRun =
    stateBytes(
      GameApp::state()
    );

  const unsigned afterFailedRunWrites =
    FakeNvs::writes;

  // 첫 OK는 실패 메시지 → 실제 야생 행동 메시지로 이동한다.
  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      firstWild->name
    )
  );

  finishFeedback();

  assert(
    GameApp::state().battle.status ==
      BattleStatus::Active
  );

  assertCommandScreen();

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      afterFailedRun &&
    FakeNvs::writes ==
      afterFailedRunWrites
  );

  // 저장된 실패 결과는 reboot에도 남지만 transient 메시지는 다시 재생하지 않는다.
  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      afterFailedRun
  );

  assertCommandScreen();

  // 싸운다 → 기존 기술 선택 화면.
  enterFight();

  assert(
    visible(
      "전기쇼크"
    ) &&
    visible(
      "울음소리"
    ) &&
    visible(
      "PP 30/30"
    )
  );

  assertMoveViewport(
    "전기쇼크",
    "울음소리",
    0
  );

  GameApp::drawGameGraphics();

  assert(
    !graphicsVisible(
      "YOU"
    ) &&
    !graphicsVisible(
      "WILD"
    ) &&
    graphicsVisible(
      firstWild->name
    )
  );

  char playerHpText[32];

  snprintf(
    playerHpText,
    sizeof(playerHpText),
    "HP %u/18",
    static_cast<unsigned>(
      GameApp::state().battle.player.currentHp
    )
  );

  assert(
    graphicsVisible(
      "피카츄"
    ) &&
    graphicsVisible(
      "Lv.5"
    ) &&
    graphicsVisible(
      playerHpText
    )
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "울음소리"
    ) &&
    visible(
      "PP 40/40"
    )
  );

  assertMoveViewport(
    "전기쇼크",
    "울음소리",
    1
  );

  // 턴 save 실패 atomicity.
  const auto beforeTurn =
    stateBytes(
      GameApp::state()
    );

  auto expectedTurn =
    GameApp::state();

  assert(
    resolveBattleTurn(
      expectedTurn,
      1
    )
  );

  FakeNvs::partialWrite = true;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      beforeTurn
  );

  assert(
    visible(
      "SAVE ERROR"
    )
  );

  assert(
    !visible(
      "변화 기술"
    ) &&
    !visible(
      "데미지!"
    )
  );

  FakeNvs::partialWrite = false;

  // 실패한 턴은 reboot 후에도 없고 명령 메뉴에서 복원된다.
  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      beforeTurn
  );

  assertCommandScreen();

  enterFight();

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      stateBytes(
        expectedTurn
      )
  );

  assert(
    visible(
      "피카츄의"
    ) &&
    visible(
      "울음소리!"
    ) &&
    visible(
      "변화 기술"
    )
  );

  const auto afterTurn =
    stateBytes(
      GameApp::state()
    );

  const unsigned afterTurnWrites =
    FakeNvs::writes;

  finishFeedback();

  // 행동 메시지가 끝나면 매 턴 전투 명령 메뉴로 돌아온다.
  if (
    GameApp::state().battle.status ==
    BattleStatus::Active
  ) {
    assertCommandScreen();

    assert(
      FakeNvs::writes ==
      afterTurnWrites
    );
  }

  // Active Battle reboot도 명령 메뉴로 복원.
  if (
    GameApp::state().battle.status ==
    BattleStatus::Active
  ) {
    GameApp::init();
    GameApp::drawGameTextScreen();

    assert(
      stateBytes(
        GameApp::state()
      ) ==
        afterTurn
    );

    assertCommandScreen();
  }

  // DESK 표시/복귀는 진행/저장을 유발하지 않는다.
  const auto idleWrites =
    FakeNvs::writes;

  const auto idleState =
    stateBytes(
      GameApp::state()
    );

  GameApp::drawDeskPet();
  GameApp::updatePokemonAnimation(
    MODE_DESK
  );

  for (
    int i = 0;
    i < 100;
    ++i
  ) {
    GameApp::update();
  }

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      idleState &&
    FakeNvs::writes ==
      idleWrites
  );

  // 승리 상태까지 실제 앱 입력으로 진행한다.
  if (
    GameApp::state().battle.status ==
    BattleStatus::Active
  ) {
    auto victory =
      GameApp::state();

    victory.battle.opponent.currentHp =
      1;

    assert(
      GameApp::saveState(
        victory
      )
    );

    GameApp::init();
    GameApp::drawGameTextScreen();

    enterFight();

    GameApp::handleButton(
      BUTTON_OK
    );

    finishFeedback();

    assert(
      GameApp::state().battle.status ==
      BattleStatus::Won
    );

    assert(
      visible(
        "전투 승리!"
      )
    );
  }

  // 결과 save 실패/재시도.
  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    visible(
      "전투 승리!"
    )
  );

  const auto wonBytes =
    stateBytes(
      GameApp::state()
    );

  FakeNvs::partialWrite = true;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      wonBytes &&
    visible(
      "OK 재시도"
    )
  );

  FakeNvs::partialWrite = false;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    GameApp::state().battle.status ==
    BattleStatus::None
  );

  assert(
    partner(
      GameApp::state()
    )->currentHp ==
    calculateStats(
      *partner(
        GameApp::state()
      )
    ).hp
  );

  // 패배 회귀.
  auto defeat =
    GameApp::state();

  assert(
    setEncounter(
      defeat.encounter,
      25,
      0,
      100,
      Gender::Male,
      false
    )
  );

  assert(
    startBattle(
      defeat
    )
  );

  defeat.battle.player.currentHp =
    1;

  assert(
    GameApp::saveState(
      defeat
    )
  );

  GameApp::init();
  GameApp::drawGameTextScreen();

  enterFight();

  GameApp::handleButton(
    BUTTON_OK
  );

  finishFeedback();

  assert(
    GameApp::state().battle.status ==
    BattleStatus::Lost
  );

  assert(
    visible(
      "쓰러졌다..."
    )
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    GameApp::state().battle.status ==
    BattleStatus::None
  );

  // PP 전부 소진 → 발버둥, 명령 메뉴는 유지.
  auto exhausted =
    GameApp::state();

  assert(
    setEncounter(
      exhausted.encounter,
      19,
      0,
      2,
      Gender::Male,
      false
    )
  );

  assert(
    startBattle(
      exhausted
    )
  );

  for (
    auto& pp :
    exhausted.battle.player.pp
  ) {
    pp = 0;
  }

  for (
    auto& pp :
    exhausted.battle.opponent.pp
  ) {
    pp = 0;
  }

  assert(
    GameApp::saveState(
      exhausted
    )
  );

  GameApp::init();
  GameApp::drawGameTextScreen();

  assertCommandScreen();

  enterFight();

  assert(
    visible(
      "발버둥"
    ) &&
    visible(
      "PP --"
    )
  );

  assertMoveViewport(
    "발버둥",
    nullptr,
    0
  );

  for (
    int i = 0;
    i < 30 &&
    GameApp::state().battle.status ==
      BattleStatus::Active;
    ++i
  ) {
    GameApp::handleButton(
      BUTTON_OK
    );

    finishFeedback();

    if (
      GameApp::state().battle.status ==
      BattleStatus::Active
    ) {
      assertCommandScreen();
      enterFight();
    }
  }

  assert(
    GameApp::state().battle.status !=
    BattleStatus::Active
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    GameApp::state().battle.status ==
    BattleStatus::None
  );

  // 4기술 viewport + command → Fight 연결 회귀.
  auto fourMoves =
    createNewGame();

  auto& four =
    fourMoves.party.members[0];

  four.moves[0] = 21;
  four.moves[1] = 33;
  four.moves[2] = 45;
  four.moves[3] = 84;

  assert(
    setEncounter(
      fourMoves.encounter,
      19,
      0,
      2,
      Gender::Male,
      false
    )
  );

  assert(
    startBattle(
      fourMoves
    )
  );

  fourMoves.battle.rngState =
    1;

  assert(
    GameApp::saveState(
      fourMoves
    )
  );

  GameApp::init();
  GameApp::drawGameTextScreen();

  assertCommandScreen();

  enterFight();

  assert(
    visible(
      "힘껏치기"
    ) &&
    visible(
      "몸통박치기"
    ) &&
    visible(
      "PP 20/20"
    )
  );

  assertMoveViewport(
    "힘껏치기",
    "몸통박치기",
    0
  );

  GameApp::handleButton(
    BUTTON_LEFT
  );

  assert(
    visible(
      "PP 30/30"
    )
  );

  assertMoveViewport(
    "울음소리",
    "전기쇼크",
    1
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "PP 35/35"
    )
  );

  assertMoveViewport(
    "힘껏치기",
    "몸통박치기",
    1
  );

  // 한 턴 후 다시 command로 돌아오는지 다시 확인.
  GameApp::handleButton(
    BUTTON_OK
  );

  finishFeedback();

  if (
    GameApp::state().battle.status ==
    BattleStatus::Active
  ) {
    assertCommandScreen();
  }

  // 마지막 테스트 상태는 Home으로 정리한다.
  while (
    GameApp::state().battle.status ==
    BattleStatus::Active
  ) {
    if (
      visible(
        "행동 선택"
      )
    ) {
      enterFight();
    }

    GameApp::handleButton(
      BUTTON_OK
    );

    finishFeedback();
  }

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    GameApp::state().battle.status ==
    BattleStatus::None
  );

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

  // G5-A2 보정: 성공 도망은 BattleState를 None으로 저장한 뒤
  // "도망 성공!" 일시 화면을 보여주고 OK에서 Home으로 돌아간다.
  auto runSuccess =
    GameApp::state();

  assert(
    setEncounter(
      runSuccess.encounter,
      19,
      0,
      2,
      Gender::Male,
      false
    )
  );

  assert(
    startBattle(
      runSuccess
    )
  );

  runSuccess.battle.rngState =
    3; // 첫 roll=7, 도망 성공.

  assert(
    GameApp::saveState(
      runSuccess
    )
  );

  GameApp::init();
  GameApp::drawGameTextScreen();

  assertCommandScreen();

  // Command index 0에서 LEFT 한 번이면 도망(index 3)으로 순환한다.
  GameApp::handleButton(
    BUTTON_LEFT
  );

  assertCommandViewport(
    "포켓몬",
    "도망",
    1
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    GameApp::state().battle.status ==
      BattleStatus::None
  );

  assert(
    visible(
      "도망 성공!"
    )
  );

  assert(
    visible(
      "OK 확인"
    )
  );

  assert(
    !visible(
      "도망 실패!"
    )
  );

  assert(
    partner(
      GameApp::state()
    )->currentHp ==
      calculateStats(
        *partner(
          GameApp::state()
        )
      ).hp
  );

  // 성공 메시지 확인은 추가 저장 없이 Home으로만 이동한다.
  const unsigned runSuccessWrites =
    FakeNvs::writes;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "피카츄"
    )
  );

  assert(
    FakeNvs::writes ==
      runSuccessWrites
  );

  // G5-B: 가방 → 볼 선택 → 포획 실패/성공 흐름.
  auto captureState =
    GameApp::state();

  assert(
    setEncounter(
      captureState.encounter,
      19,
      0,
      2,
      Gender::Male,
      false
    )
  );

  assert(
    startBattle(
      captureState
    )
  );

  captureState.progress.ballTier =
    2;

  captureState.progress.masterBallCount =
    1;

  captureState.battle.rngState =
    4; // 첫 roll=76, 풀피 꼬렛 몬스터볼 60%에서 실패.

  assert(
    GameApp::saveState(
      captureState
    )
  );

  GameApp::init();
  GameApp::drawGameTextScreen();

  assertCommandScreen();

  enterBag();

  // ballTier 2 + 마스터볼 1개: 일반 3종 + 마스터볼 + 돌아가기.
  assert(
    visible(
      "몬스터볼"
    ) &&
    visible(
      "슈퍼볼"
    )
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "하이퍼볼"
    )
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "마스터볼"
    ) &&
    visible(
      "x1"
    )
  );

  // 다시 몬스터볼로 순환한다.
  GameApp::handleButton(
    BUTTON_RIGHT
  );

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "몬스터볼"
    )
  );

  const auto beforeCapture =
    stateBytes(
      GameApp::state()
    );

  const unsigned beforeCaptureWrites =
    FakeNvs::writes;

  // 포획 결과 저장 실패 시 RNG/HP/PP/turn/마스터볼 수량 모두 적용되지 않는다.
  FakeNvs::partialWrite =
    true;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      beforeCapture
  );

  assert(
    visible(
      "SAVE ERROR"
    )
  );

  assert(
    !visible(
      "포획 실패!"
    ) &&
    !visible(
      "포획 성공!"
    ) &&
    !visible(
      "포획 중..."
    )
  );

  FakeNvs::partialWrite =
    false;

  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      beforeCapture
  );

  assertCommandScreen();

  // 같은 seed로 재시도하면 같은 포획 실패 + 상대 반격이 저장된다.
  enterBag();

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "몬스터볼"
    ) &&
    visible(
      "포획 중..."
    )
  );

  // 저장된 결과는 연출 중에도 이미 적용되어 있다.
  assert(
    GameApp::state().battle.status ==
      BattleStatus::Active
  );

  assert(
    GameApp::state().battle.turn ==
      1
  );

  assert(
    GameApp::state().progress.masterBallCount ==
      1
  );

  const auto afterFailedCapture =
    stateBytes(
      GameApp::state()
    );

  const unsigned afterFailedCaptureWrites =
    FakeNvs::writes;

  // 실패는 1~2회 흔들린 뒤 기존 실패 안내로 이어진다.
  finishCaptureAnimation(
    "포획 실패!"
  );

  assert(
    visible(
      "상대가 공격한다"
    )
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "꼬렛의"
    )
  );

  finishFeedback();

  assert(
    GameApp::state().battle.status ==
      BattleStatus::Active
  );

  assertCommandScreen();

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      afterFailedCapture &&
    FakeNvs::writes ==
      afterFailedCaptureWrites
  );

  // transient 실패/공격 메시지는 reboot 후 다시 재생하지 않는다.
  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      afterFailedCapture
  );

  assertCommandScreen();

  // 같은 전투를 성공 seed로 바꿔 G5-B 성공 UI를 검증한다.
  auto captureSuccess =
    GameApp::state();

  captureSuccess.battle.rngState =
    3; // 첫 roll=7, 몬스터볼 포획 성공.

  assert(
    GameApp::saveState(
      captureSuccess
    )
  );

  GameApp::init();
  GameApp::drawGameTextScreen();

  assertCommandScreen();

  const uint8_t partyBeforeCapture =
    GameApp::state().party.count;

  assert(
    !dexContains(
      GameApp::state().pokedex.caught,
      19
    )
  );

  enterBag();

  // 일반 볼 성공 RNG 후보도 readback 검증 실패 시 소유/ID/도감에 적용하지 않는다.
  const auto beforeSuccess = stateBytes(GameApp::state());
  FakeNvs::corruptWrite = true;
  GameApp::handleButton(BUTTON_OK);
  assert(stateBytes(GameApp::state()) == beforeSuccess && visible("SAVE ERROR"));
  assert(!visible("포획 중...") && !visible("파티에 합류!"));
  FakeNvs::corruptWrite = false;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    GameApp::state().battle.status ==
      BattleStatus::None
  );

  assert(
    visible(
      "몬스터볼"
    ) &&
    visible(
      "포획 중..."
    )
  );

  // 성공은 3회 흔들린 뒤 결과 화면으로 넘어간다(기존 350/420/250ms 유지).
  const unsigned long animationStart = hostMillis;
  const auto animationWrites = FakeNvs::writes;
  for (unsigned shake = 0; shake < 3; ++shake) {
    hostMillis = animationStart + 350 + shake * 420;
    GameApp::updatePokemonAnimation(MODE_GAME);
    assert(visible("포획 중..."));
    int left = 128;
    for (const auto& pixel : deskOled.framePixels) if (pixel.first < left) left = pixel.first;
    assert(left == 53);
    hostMillis += 120;
    GameApp::updatePokemonAnimation(MODE_GAME);
    left = 128;
    for (const auto& pixel : deskOled.framePixels) if (pixel.first < left) left = pixel.first;
    assert(left == 59);
  }
  hostMillis = animationStart + 350 + 3 * 420 + 249;
  GameApp::updatePokemonAnimation(MODE_GAME);
  assert(visible("포획 중...") && FakeNvs::writes == animationWrites);
  finishCaptureAnimation(
    "포획 성공!"
  );

  assert(
    visible(
      "꼬렛"
    ) &&
    visible(
      "파티에 합류!"
    )
  );

  // C1: 성공 저장 시점에 실제 파티/도감에 반영된다.
  assert(
    GameApp::state().party.count ==
      partyBeforeCapture + 1
  );

  assert(
    dexContains(
      GameApp::state().pokedex.caught,
      19
    )
  );

  const unsigned captureSuccessWrites =
    FakeNvs::writes;

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "피카츄"
    )
  );

  assert(
    FakeNvs::writes ==
      captureSuccessWrites
  );

  // C1.1: 포획 직후 파티 조회에서 실제 두 번째 개체를 확인한다.
  // GameApp::init()이 홈 메뉴 인덱스를 상태(0)로 초기화하므로 RIGHT 한 번이면 파티(1)다.
  GameApp::handleButton(
    BUTTON_RIGHT
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "파티 1/2"
    ) &&
    visible(
      "피카츄"
    )
  );

  const unsigned ownedPartyWrites =
    FakeNvs::writes;

  GameApp::handleButton(
    BUTTON_RIGHT
  );

  assert(
    visible(
      "파티 2/2"
    ) &&
    visible(
      "꼬렛"
    ) &&
    FakeNvs::writes ==
      ownedPartyWrites
  );

  GameApp::handleButton(
    BUTTON_LEFT
  );

  assert(
    visible(
      "파티 1/2"
    ) &&
    visible(
      "피카츄"
    )
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  // 성공 결과는 이미 Battle None으로 저장됐으므로 reboot 후 Home이다.
  GameApp::init();
  GameApp::drawGameTextScreen();

  assert(
    visible(
      "피카츄"
    )
  );

  assert(
    GameApp::state().battle.status ==
      BattleStatus::None
  );

  assert(
    FakeNvs::writes >
      beforeCaptureWrites
  );

  // 성공 후보 저장 실패: 소유/도감/ID/마스터볼/RNG 전체가 이전 바이트와 같아야 한다.
  assert(GameApp::state().party.count == 2 && dexContains(GameApp::state().pokedex.caught,19));
  assert(!dexContains(GameApp::state().pokedex.shinyCaught,19));
  auto masterCandidate = GameApp::state();
  assert(setEncounter(masterCandidate.encounter,16,0,7,Gender::Female,true));
  assert(startBattle(masterCandidate));
  masterCandidate.battle.opponent.currentHp = 2;
  masterCandidate.progress.masterBallCount = 1;
  assert(GameApp::saveState(masterCandidate)); GameApp::init(); GameApp::drawGameTextScreen();
  enterBag();
  for (int i=0;i<3;++i) GameApp::handleButton(BUTTON_RIGHT);
  assert(visible("마스터볼"));
  const auto beforeMaster = stateBytes(GameApp::state());
  const auto nextId = GameApp::state().progress.nextInstanceId;
  FakeNvs::partialWrite = true;
  GameApp::handleButton(BUTTON_OK);
  assert(stateBytes(GameApp::state()) == beforeMaster && visible("SAVE ERROR"));
  assert(!visible("포획 중...") && !visible("포획 성공!") && !visible("파티에 합류!"));
  const auto failedMasterWrites = FakeNvs::writes;
  for (int i=0;i<50;++i) { hostMillis += 100; GameApp::updatePokemonAnimation(MODE_GAME); GameApp::update(); }
  assert(FakeNvs::writes == failedMasterWrites && stateBytes(GameApp::state()) == beforeMaster);
  FakeNvs::partialWrite = false;
  GameApp::handleButton(BUTTON_OK);
  assert(FakeNvs::writes == failedMasterWrites + 1 && visible("포획 중..."));
  assert(GameApp::state().party.count == 3 && GameApp::state().progress.nextInstanceId == nextId + 1);
  const auto& owned = GameApp::state().party.members[2];
  assert(owned.instanceId == nextId && owned.speciesId == 16 && owned.level == 7);
  assert(owned.gender == Gender::Female && owned.shiny && owned.currentHp == 2);
  assert(GameApp::state().progress.masterBallCount == 0 && dexContains(GameApp::state().pokedex.shinyCaught,16));
  const auto capturedState = GameApp::state();
  const auto capturedBytes = stateBytes(capturedState);
  GameApp::handleButton(BUTTON_OK); // 연출 중 확인 입력으로 중복 추가하지 않는다.
  assert(stateBytes(GameApp::state()) == capturedBytes);
  GameApp::init(); GameApp::drawGameTextScreen(); // 성공 연출 도중 재부팅.
  assert(stateBytes(GameApp::state()) == capturedBytes && visible("피카츄") && !visible("포획 중..."));
  assert(partner(GameApp::state())->instanceId == 1 && GameApp::state().progress.deskPetId == 1);

  auto fullBattle = capturedState;
  assert(setEncounter(fullBattle.encounter,19,0,2,Gender::Male,false) && startBattle(fullBattle));
  fullBattle.progress.masterBallCount = 1;
  assert(GameApp::saveState(fullBattle)); GameApp::init(); GameApp::drawGameTextScreen(); enterBag();
  const auto beforeFullNvs = FakeNvs::data;
  const auto beforeFullFiles = FakeLittleFS::files;
  GameSave fullSave;
  assert(GameSaveStorage::load(fullSave) == GameSaveStorage::LoadResult::Loaded);
  installCaptureBoxFixture(fullSave, BOX_CAPACITY, fullSave.boxRoot.generation + 1);
  GameApp::init(); GameApp::drawGameTextScreen(); enterBag();
  const auto fullBytes = stateBytes(GameApp::state());
  const auto fullWrites = FakeNvs::writes;
  GameApp::handleButton(BUTTON_OK);
  assert(visible("박스가 가득 찼다") && visible("OK 확인"));
  GameApp::handleButton(BUTTON_RIGHT); // 안내 중 선택 입력은 무시.
  for (int i=0;i<30;++i) { GameApp::update(); GameApp::updatePokemonAnimation(MODE_GAME); }
  assert(stateBytes(GameApp::state()) == fullBytes && FakeNvs::writes == fullWrites);
  GameApp::handleButton(BUTTON_OK); assert(visible("몬스터볼"));
  for (int i=0;i<3;++i) GameApp::handleButton(BUTTON_RIGHT);
  assert(visible("마스터볼")); GameApp::handleButton(BUTTON_OK);
  assert(visible("박스가 가득 찼다") && stateBytes(GameApp::state()) == fullBytes);
  assert(FakeNvs::writes == fullWrites);
  GameApp::init(); GameApp::drawGameTextScreen(); assertCommandScreen();
  assert(stateBytes(GameApp::state()) == fullBytes);

  FakeNvs::data = beforeFullNvs; FakeLittleFS::files = beforeFullFiles;
  GameApp::init();

  auto exhaustedId = createNewGame();
  assert(setEncounter(exhaustedId.encounter,19,0,2,Gender::Male,false) && startBattle(exhaustedId));
  exhaustedId.progress.nextInstanceId = UINT32_MAX;
  assert(GameApp::saveState(exhaustedId)); GameApp::init(); GameApp::drawGameTextScreen(); enterBag();
  const auto exhaustedBytes = stateBytes(GameApp::state());
  const auto exhaustedWrites = FakeNvs::writes;
  GameApp::handleButton(BUTTON_OK);
  assert(visible("개체 ID 부족") && stateBytes(GameApp::state()) == exhaustedBytes);
  assert(FakeNvs::writes == exhaustedWrites);
  GameApp::handleButton(BUTTON_OK); assert(visible("몬스터볼"));
  // 독립적인 거부 시나리오 후, 앞서 저장 성공한 소유 fixture로 복귀해 재로드 검증.
  assert(GameApp::saveState(capturedState)); GameApp::init(); GameApp::drawGameTextScreen();
  assert(stateBytes(GameApp::state()) == capturedBytes);
  std::puts("PASS app C1: atomic ownership/master retry, exactly-once IDs, full-Box no-save, animation/reboot ownership");

  // Both NVS roots reference unavailable Box data: fallback display must never
  // create a replacement save or enable writes over the existing records.
  const auto protectedNvs = FakeNvs::data;
  const auto protectedWrites = FakeNvs::writes;
  const auto protectedFiles = FakeLittleFS::files;
  FakeLittleFS::files.clear();
  GameApp::init(); GameApp::drawGameTextScreen();
  assert(visible("저장 오류") && FakeNvs::data == protectedNvs);
  assert(!GameApp::saveState(createNewGame()));
  assert(FakeNvs::writes == protectedWrites && FakeNvs::data == protectedNvs);
  FakeLittleFS::files = protectedFiles;
  GameApp::init();
  assert(stateBytes(GameApp::state()) == capturedBytes);

  const auto beforeUncertain = stateBytes(GameApp::state());
  auto uncertainCandidate = GameApp::state();
  ++uncertainCandidate.progress.playTimeSeconds;
  FakeNvs::failReadAfterWrite = true;
  assert(!GameApp::saveState(uncertainCandidate));
  assert(stateBytes(GameApp::state()) == beforeUncertain);
  const auto uncertainWrites = FakeNvs::writes;
  FakeNvs::failReadAfterWrite = FakeNvs::failRead = false;
  assert(!GameApp::saveState(uncertainCandidate) && FakeNvs::writes == uncertainWrites);
  GameApp::init(); // Readback uncertainty resolved by actual A/B pair load.
  assert(stateBytes(GameApp::state()) == stateBytes(uncertainCandidate));
  std::puts("PASS app B2: incomplete pair protects NVS, uncertain commit keeps live state and blocks retry until reload");

  const unsigned finalWrites =
    FakeNvs::writes;

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
    "PASS app C1.1: party viewer, C1 ownership, capture animation, full-Box guard, reboot and atomic save"
  );
  // Independent C fixtures exercise the real coordinator and UI transaction gates.
  for (unsigned scenario = 0; scenario < 3; ++scenario) {
    resetStorageFakes(); GameApp::init();
    auto next = GameApp::state();
    while (next.party.count < PARTY_CAPACITY) {
      auto pokemon = next.party.members[0];
      pokemon.instanceId = next.progress.nextInstanceId++;
      next.party.members[next.party.count++] = pokemon;
    }
    next.progress.ballTier = 2; next.progress.masterBallCount = 1;
    assert(setEncounter(next.encounter,19,0,7,Gender::Female,true) && startBattle(next));
    next.battle.opponent.currentHp = 3;
    assert(GameApp::saveState(next)); GameApp::init(); GameApp::drawGameTextScreen(); enterBag();
    for (int i = 0; i < 3; ++i) GameApp::handleButton(BUTTON_RIGHT);
    assert(visible("마스터볼"));
    const auto before = stateBytes(GameApp::state());
    if (scenario == 0) FakeNvs::rejectWrite = true;
    if (scenario == 1) FakeNvs::failReadAfterWrite = true;
    if (scenario == 2) FakeLittleFS::writeBudget = 100;
    GameApp::handleButton(BUTTON_OK);
    assert(visible("SAVE ERROR") && !visible("포획 중...") && !visible("박스로 전송!"));
    assert(stateBytes(GameApp::state()) == before);
    assert(FakeLittleFS::files.size() == 2);
    FakeNvs::rejectWrite = FakeNvs::failReadAfterWrite = FakeNvs::failRead = false;
    FakeLittleFS::writeBudget = std::numeric_limits<size_t>::max();
    const auto writes = FakeNvs::writes;
    GameApp::handleButton(BUTTON_OK);
    if (scenario == 1) {
      assert(visible("SAVE ERROR") && stateBytes(GameApp::state()) == before);
      assert(FakeNvs::writes == writes && FakeLittleFS::files.size() == 2);
    } else {
      assert(visible("포획 중...") && FakeNvs::writes == writes + 1);
      assert(GameApp::state().party.count == 3);
      finishCaptureAnimation("포획 성공!");
      assert(visible("박스로 전송!") && !visible("파티에 합류!"));
      GameApp::handleButton(BUTTON_OK);
    }
    GameApp::init(); GameApp::drawGameTextScreen();
    assert(visible("피카츄") && !visible("포획 성공!") && !visible("포획 중..."));
    assert(GameApp::state().party.count == 3 && GameApp::state().battle.status == BattleStatus::None);
    assert(GameApp::state().progress.masterBallCount == 0 && GameApp::state().progress.nextInstanceId == 5);
    GameSave loaded;
    assert(GameSaveStorage::load(loaded) == GameSaveStorage::LoadResult::Loaded);
    assert(loaded.boxRoot.occupiedCount == 1);
    BoxStorage::Snapshot box;
    assert(box.open({loaded.boxRoot.storeId,loaded.boxRoot.generation}) == BoxStorage::Result::Ok);
    PokemonInstance caught;
    assert(box.readSlot(0,caught) == BoxStorage::Result::Ok);
    assert(caught.instanceId == 4 && caught.speciesId == 19 && caught.shiny && caught.currentHp == 3);
  }
  std::puts("PASS app C: full Box/ID notices, Box success animation/message, write failures/retry, Indeterminate lock/reboot");

}
