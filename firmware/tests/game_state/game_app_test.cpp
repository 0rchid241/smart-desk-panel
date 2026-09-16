#include "game_app.h"
#include "save_storage.h"
#include "Preferences.h"

#include <Adafruit_SSD1306.h>

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

int main() {
  using namespace PokemonGame;

  auto stateBytes =
    [](
      const GameState& state
    ) {
      GameSave save;
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

  FakeNvs::reset();

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

  // 가방 placeholder.
  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    visible(
      "가방"
    ) &&
    visible(
      "아직 준비 중"
    )
  );

  GameApp::handleButton(
    BUTTON_LEFT
  );

  assert(
    visible(
      "아직 준비 중"
    )
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

  // 도망 placeholder.
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
    visible(
      "도망"
    ) &&
    visible(
      "아직 준비 중"
    )
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    stateBytes(
      GameApp::state()
    ) ==
      commandState &&
    FakeNvs::writes ==
      commandWrites
  );

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

  assert(
    graphicsVisible(
      "피카츄"
    ) &&
    graphicsVisible(
      "Lv.5"
    ) &&
    graphicsVisible(
      "HP 18/18"
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
    "PASS app G5-A1: command menu, placeholders, Fight submenu, turn feedback, reboot and atomic save"
  );
}
