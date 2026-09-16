#include "game_app.h"
#include "encounter.h"
#include "save_storage.h"
#include "../hardware/displays.h"
#include "../core/app_config.h"
#include "../services/network_time.h"
#include "../../hangul_renderer.h"

#include <cstdio>

// 로컬 전용 포켓몬 애셋.
// 이 파일은 Git에 올리지 않아도 공개 저장소가 컴파일되도록 조건부 포함한다.
#if __has_include("../../local_game_assets/pokemon/pikachu_idle_1bit.h")
  #include "../../local_game_assets/pokemon/pikachu_idle_1bit.h"
  #define HAS_LOCAL_PIKACHU_ASSET 1
#else
  #define HAS_LOCAL_PIKACHU_ASSET 0
#endif

// 야생 조우/전투용 로컬 스프라이트.
// 공개 저장소에는 포함하지 않고, 없으면 fallback으로 동작한다.
#if __has_include("../../local_game_assets/pokemon/wild_encounter_1bit.h")
  #include "../../local_game_assets/pokemon/wild_encounter_1bit.h"
  #define HAS_LOCAL_WILD_ASSET 1
#else
  #define HAS_LOCAL_WILD_ASSET 0
#endif

using namespace AppConfig;

namespace GameApp {
namespace {

#if HAS_LOCAL_PIKACHU_ASSET
const uint16_t PIKACHU_IDLE_FRAME_DURATION_MS[
  PIKACHU_IDLE_FRAME_COUNT
] = {
  2000,
  100,
  150,
  150,
  150,
  100
};

int pokemonIdleFrame = 0;
unsigned long pokemonIdleFrameStartedAt = 0;
#endif

const char* GAME_MENU_ITEMS[] = {
  "상태",
  "파티",
  "탐험",
  "도감"
};

const int GAME_MENU_ITEM_COUNT =
  sizeof(GAME_MENU_ITEMS) /
  sizeof(GAME_MENU_ITEMS[0]);

const char* BATTLE_COMMAND_ITEMS[] = {
  "싸운다",
  "가방",
  "포켓몬",
  "도망"
};

constexpr uint8_t BATTLE_COMMAND_COUNT = 4;

int gameMenuIndex = 0;
uint8_t partyViewIndex = 0;

PokemonGame::GameSave gameSave;

enum class GameScreen {
  Home,
  Status,
  Party,
  RegionSelect,
  Exploring,

  // 정상 흐름에서는 곧바로 WildEncounter로 간다.
  // 구버전 Complete 저장을 복구하거나 저장 실패를 재시도할 때만 사용한다.
  ExplorationComplete,

  WildEncounter,
  Battle,
  RunSuccess,
  CaptureAnimation,
  CaptureSuccess
};

enum class BattleUiMode : uint8_t {
  Command,
  MoveSelection,
  BallSelection,
  PartyUnavailable,
  RunFailed,
  CaptureFailed,
  CaptureUnavailable
};

GameScreen screen = GameScreen::Home;

BattleUiMode battleUiMode = BattleUiMode::Command;
uint8_t battleCommandIndex = 0;
uint8_t battleCommandViewportTop = 0;
uint8_t battleMoveSlot = 0;
uint8_t battleMoveViewportTop = 0;
uint8_t battleBallIndex = 0;
uint8_t battleBallViewportTop = 0;
PokemonGame::BattleTurnReport battleReport;
PokemonGame::BattleCaptureReport battleCaptureReport;
uint8_t battleActionIndex = 0;
bool hitEffect = false;
unsigned long hitEffectStarted = 0;
unsigned long hitEffectFrame = 0;

// G5-B.1 포획 연출은 저장하지 않는 transient UI 상태다.
// 판정/반격 결과는 이미 저장된 뒤 이 타이머 연출만 재생된다.
unsigned long captureAnimationStartedAt = 0;
uint8_t captureAnimationShakeCount = 0;
uint8_t captureAnimationVisualFrame = 0xff;

constexpr unsigned long CAPTURE_INTRO_MS = 350;
constexpr unsigned long CAPTURE_SHAKE_MS = 420;
constexpr unsigned long CAPTURE_SETTLE_MS = 250;

bool saveError = false;

// 탐험 완료 저장에 실패했을 때 매 loop마다 flash 재시도하지 않는다.
// 사용자가 OK를 눌렀을 때만 다시 시도한다.
bool completionSaveBlocked = false;

// 0: 공개 테스트 지역
// 1: 돌아가기
int regionChoice = 0;

const char* regionMessage = nullptr;

// GAME 그래픽 OLED가 현재 GameScreen과 다른 내용을 보여줄 수 있음을 표시한다.
// update()는 DESK 모드에서도 실행되므로 직접 OLED를 그리지 않고 dirty만 세운다.
bool graphicsDirty = true;

void resetBattleCommandUi() {
  battleUiMode = BattleUiMode::Command;
  battleCommandIndex = 0;
  battleCommandViewportTop = 0;
}

const char* captureBallName(
  PokemonGame::CaptureBall ball
) {
  using PokemonGame::CaptureBall;

  switch (ball) {
    case CaptureBall::Poke:
      return "몬스터볼";

    case CaptureBall::Great:
      return "슈퍼볼";

    case CaptureBall::Ultra:
      return "하이퍼볼";

    case CaptureBall::Master:
      return "마스터볼";

    default:
      return "?";
  }
}

uint8_t availableBattleBalls(
  PokemonGame::CaptureBall (&balls)[4]
) {
  using namespace PokemonGame;

  uint8_t count = 0;

  balls[count++] =
    CaptureBall::Poke;

  if (
    gameSave.state.progress.ballTier >=
    1
  ) {
    balls[count++] =
      CaptureBall::Great;
  }

  if (
    gameSave.state.progress.ballTier >=
    2
  ) {
    balls[count++] =
      CaptureBall::Ultra;
  }

  if (
    gameSave.state.progress.masterBallCount >
    0
  ) {
    balls[count++] =
      CaptureBall::Master;
  }

  return count;
}

void resetBattleBallUi() {
  battleBallIndex = 0;
  battleBallViewportTop = 0;
}

unsigned long captureAnimationDuration() {
  return
    CAPTURE_INTRO_MS +
    static_cast<unsigned long>(
      captureAnimationShakeCount
    ) *
      CAPTURE_SHAKE_MS +
    CAPTURE_SETTLE_MS;
}

uint8_t captureVisualFrameForElapsed(
  unsigned long elapsed
) {
  if (
    elapsed <
    CAPTURE_INTRO_MS
  ) {
    return 0;
  }

  elapsed -=
    CAPTURE_INTRO_MS;

  const unsigned long shakeSpan =
    static_cast<unsigned long>(
      captureAnimationShakeCount
    ) *
    CAPTURE_SHAKE_MS;

  if (
    elapsed >=
    shakeSpan
  ) {
    return 0;
  }

  const unsigned long withinShake =
    elapsed %
    CAPTURE_SHAKE_MS;

  if (
    withinShake <
    120
  ) {
    return 1;
  }

  if (
    withinShake <
    240
  ) {
    return 2;
  }

  return 0;
}

int captureBallOffsetX() {
  switch (
    captureAnimationVisualFrame
  ) {
    case 1:
      return -3;

    case 2:
      return 3;

    default:
      return 0;
  }
}

void beginCaptureAnimation() {
  // 성공은 원작 느낌에 가깝게 3회 흔들고,
  // 실패는 확률이 높을수록 한 번 더 버티는 연출만 준다.
  // 실제 포획 판정에는 영향을 주지 않는다.
  captureAnimationShakeCount =
    battleCaptureReport.captured
      ? 3
      : (
          battleCaptureReport.chance >=
          60
            ? 2
            : 1
        );

  captureAnimationStartedAt =
    millis();

  captureAnimationVisualFrame =
    0xff;

  screen =
    GameScreen::CaptureAnimation;

  hitEffect = false;
  graphicsDirty = true;
}

void finishCaptureAnimation() {
  captureAnimationVisualFrame = 0;
  graphicsDirty = true;

  if (
    battleCaptureReport.captured
  ) {
    screen =
      GameScreen::CaptureSuccess;

    return;
  }

  screen =
    GameScreen::Battle;

  battleActionIndex =
    battleReport.count;

  battleUiMode =
    BattleUiMode::CaptureFailed;
}

void drawCaptureBallIcon(
  Adafruit_SSD1306& oled,
  int16_t centerX,
  int16_t topY
) {
  // 16x16 흑백 포획볼 아이콘. 색상 구분 대신 text OLED의 볼 이름을 사용한다.
  static const uint16_t rows[16] = {
    0x07e0,
    0x1818,
    0x2004,
    0x4002,
    0x8001,
    0x8001,
    0xffff,
    0x83c1,
    0x8241,
    0xffff,
    0x8001,
    0x8001,
    0x4002,
    0x2004,
    0x1818,
    0x07e0
  };

  const int16_t left =
    static_cast<int16_t>(
      centerX -
      8
    );

  for (
    int16_t y = 0;
    y < 16;
    ++y
  ) {
    const uint16_t bits =
      rows[y];

    for (
      int16_t x = 0;
      x < 16;
      ++x
    ) {
      if (
        bits &
        (
          0x8000u >>
          x
        )
      ) {
        oled.drawPixel(
          left + x,
          topY + y,
          SSD1306_WHITE
        );
      }
    }
  }
}

void drawCaptureAnimationGraphic(
  Adafruit_SSD1306& oled,
  bool successResult
) {
  oled.clearDisplay();
  oled.setTextSize(
    1
  );
  oled.setTextColor(
    SSD1306_WHITE
  );

  const int16_t centerX =
    static_cast<int16_t>(
      64 +
      (
        successResult
          ? 0
          : captureBallOffsetX()
      )
    );

  drawCaptureBallIcon(
    oled,
    centerX,
    24
  );

  oled.drawLine(
    46,
    43,
    82,
    43,
    SSD1306_WHITE
  );

  if (
    successResult
  ) {
    // 성공 직후의 작은 "딸깍" 느낌을 주는 정적 반짝임.
    oled.drawPixel(
      48,
      22,
      SSD1306_WHITE
    );
    oled.drawPixel(
      80,
      22,
      SSD1306_WHITE
    );
    oled.drawPixel(
      64,
      18,
      SSD1306_WHITE
    );
  }

  oled.display();
}

void initGameState() {
  using GameSaveStorage::LoadResult;

  const auto result =
    GameSaveStorage::load(
      gameSave
    );

  if (result == LoadResult::Loaded) {
    Serial.println(
      "Game save loaded"
    );

    if (
      gameSave.saveVersion !=
      PokemonGame::SAVE_VERSION
    ) {
      // 이전 버전 상태는 RAM에서 그대로 유지한다.
      // 저장이 성공하면 현재 SAVE_VERSION으로 승격된다.
      saveError =
        GameSaveStorage::initialize(
          gameSave
        ) != GameSaveStorage::CommitResult::Committed;

      Serial.println(
        saveError
          ? "Game migration save failed; loaded state kept"
          : "Game save migrated to current version"
      );
    }

    return;
  }

  gameSave = PokemonGame::GameSave{};
  gameSave.state =
    PokemonGame::createNewGame();

  if (
    result == LoadResult::Missing
  ) {
    Serial.println(
      "Game: first save"
    );

    saveError =
      GameSaveStorage::initialize(
        gameSave
      ) != GameSaveStorage::CommitResult::Committed;
  } else {
    // 손상된 main/Box pair, I/O 오류, newer save는 기존 bytes를 덮어쓰지 않는다.
    saveError = true;

    Serial.println(
      result == LoadResult::UnsupportedVersion
        ? "Game: unsupported save version"
        : "Game: storage unavailable"
    );
  }

  Serial.println(
    saveError
      ? "Game save failed (RAM only)"
      : "Game save created"
  );
}

bool prepareEncounterForCompletedExploration(
  PokemonGame::GameState& state
) {
  using namespace PokemonGame;

  if (
    state.exploration.status !=
    ExplorationStatus::Complete
  ) {
    return false;
  }

  // 이미 조우가 확정되어 있다면 다시 뽑지 않는다.
  if (
    state.encounter.status ==
    EncounterStatus::Ready
  ) {
    return true;
  }

  if (
    state.encounter.status !=
    EncounterStatus::None
  ) {
    return false;
  }

  const uint32_t roll =
    makeTestEncounterRoll(
      state.exploration
    );

  return resolveTestEncounter(
    state.encounter,
    roll
  );
}

bool persistEncounterForCompletedExploration() {
  PokemonGame::GameState next =
    gameSave.state;

  if (
    !prepareEncounterForCompletedExploration(
      next
    )
  ) {
    saveError = true;

    Serial.println(
      "Game encounter preparation failed"
    );

    return false;
  }

  if (
    !saveState(
      next
    )
  ) {
    return false;
  }

  screen =
    GameScreen::WildEncounter;

  graphicsDirty = true;
  completionSaveBlocked = false;

  return true;
}

void drawPokemonGraphic(
  Adafruit_SSD1306& target
) {
  target.clearDisplay();

#if HAS_LOCAL_PIKACHU_ASSET
  const int x =
    (
      SCREEN_WIDTH -
      PIKACHU_IDLE_FRAME_WIDTH
    ) / 2;

  const int y =
    (
      SCREEN_HEIGHT -
      PIKACHU_IDLE_FRAME_HEIGHT
    ) / 2;

  target.drawBitmap(
    x,
    y,
    pikachu_idle_frames[
      pokemonIdleFrame
    ],
    PIKACHU_IDLE_FRAME_WIDTH,
    PIKACHU_IDLE_FRAME_HEIGHT,
    SSD1306_WHITE
  );
#else
  target.setTextColor(
    SSD1306_WHITE
  );

  target.setTextSize(
    1
  );

  drawUtf8Text(
    target,
    0,
    0,
    "포켓몬"
  );

  target.drawLine(
    0,
    18,
    127,
    18,
    SSD1306_WHITE
  );

  drawUtf8Text(
    target,
    12,
    26,
    "로컬 그림"
  );

  drawUtf8Text(
    target,
    21,
    46,
    "없음"
  );
#endif

  target.display();
}

#if HAS_LOCAL_WILD_ASSET
const uint8_t* localWildSprite(
  PokemonGame::SpeciesId speciesId
) {
  switch (speciesId) {
    case 16:
      return pidgey_wild_1bit;

    case 19:
      return rattata_wild_1bit;

    case 25:
      return pikachu_wild_1bit;

    default:
      return nullptr;
  }
}
#endif

void drawWildEncounterGraphic(
  Adafruit_SSD1306& target
) {
  using namespace PokemonGame;

  target.clearDisplay();

  target.setTextColor(
    SSD1306_WHITE
  );

  target.setTextSize(
    1
  );

  const auto& encounter =
    gameSave.state.encounter;

  const auto* species =
    findSpecies(
      encounter.speciesId,
      encounter.formId
    );

  if (
    encounter.status !=
      EncounterStatus::Ready ||
    !species
  ) {
    target.setCursor(
      0,
      0
    );

    target.print(
      "WILD ?"
    );

    drawUtf8Text(
      target,
      0,
      24,
      "조우 오류"
    );

    target.display();

    return;
  }

#if HAS_LOCAL_WILD_ASSET
  const uint8_t* sprite =
    localWildSprite(
      encounter.speciesId
    );

  if (
    sprite != nullptr &&
    encounter.formId == 0
  ) {
    const int x =
      (
        SCREEN_WIDTH -
        WILD_SPRITE_WIDTH
      ) / 2;

    const int y =
      (
        SCREEN_HEIGHT -
        WILD_SPRITE_HEIGHT
      ) / 2;

    target.drawBitmap(
      x,
      y,
      sprite,
      WILD_SPRITE_WIDTH,
      WILD_SPRITE_HEIGHT,
      SSD1306_WHITE
    );

    target.drawLine(
      8,
      59,
      119,
      59,
      SSD1306_WHITE
    );

    target.display();

    return;
  }
#endif

  // 로컬 스프라이트가 없는 공개 빌드용 fallback.
  target.setCursor(
    0,
    0
  );

  target.print(
    "WILD"
  );

  char idText[16];

  snprintf(
    idText,
    sizeof(idText),
    "#%03u",
    static_cast<unsigned>(
      encounter.speciesId
    )
  );

  target.setCursor(
    92,
    0
  );

  target.print(
    idText
  );

  target.drawLine(
    0,
    11,
    127,
    11,
    SSD1306_WHITE
  );

  target.setTextSize(
    2
  );

  target.setCursor(
    8,
    22
  );

  target.print(
    "?"
  );

  target.setTextSize(
    1
  );

  drawUtf8Text(
    target,
    38,
    20,
    species->name
  );

  char levelText[16];

  snprintf(
    levelText,
    sizeof(levelText),
    "Lv.%u",
    static_cast<unsigned>(
      encounter.level
    )
  );

  target.setCursor(
    38,
    44
  );

  target.print(
    levelText
  );

  target.drawLine(
    8,
    59,
    119,
    59,
    SSD1306_WHITE
  );

  target.display();
}

void drawWildEncounterText(
  Adafruit_SSD1306& oled
) {
  using namespace PokemonGame;

  const auto& encounter =
    gameSave.state.encounter;

  const auto* species =
    findSpecies(
      encounter.speciesId,
      encounter.formId
    );

  if (
    encounter.status !=
      EncounterStatus::Ready ||
    !species
  ) {
    drawUtf8Text(
      oled,
      0,
      0,
      "조우 오류"
    );

    drawUtf8Text(
      oled,
      0,
      48,
      "OK"
    );

    return;
  }

  drawUtf8Text(
    oled,
    0,
    0,
    "야생의"
  );

  char encounterLine[48];

  snprintf(
    encounterLine,
    sizeof(encounterLine),
    "%s 등장!",
    species->name
  );

  drawUtf8Text(
    oled,
    0,
    24,
    encounterLine
  );

  if (
    saveError
  ) {
    drawUtf8Text(
      oled,
      0,
      48,
      "OK 재시도"
    );

    return;
  }

  char bottomLine[32];

  snprintf(
    bottomLine,
    sizeof(bottomLine),
    "Lv.%u        OK",
    static_cast<unsigned>(
      encounter.level
    )
  );

  oled.setCursor(
    0,
    52
  );

  oled.print(
    bottomLine
  );
}

void selectUsableBattleMove() {
  const auto* p =
    PokemonGame::partner(
      gameSave.state
    );

  if (
    p &&
    !PokemonGame::canSelectMove(
      *p,
      gameSave.state.battle,
      battleMoveSlot
    )
  ) {
    battleMoveSlot =
      PokemonGame::nextBattleMove(
        *p,
        gameSave.state.battle,
        battleMoveSlot,
        1
      );
  }
}

bool showingBattleAction() {
  return
    battleActionIndex <
    battleReport.count;
}

void beginActionFeedback() {
  hitEffect =
    showingBattleAction() &&
    battleReport.actions[
      battleActionIndex
    ].hit &&
    battleReport.actions[
      battleActionIndex
    ].damage > 0;

  hitEffectStarted = millis();
  hitEffectFrame = 0;
  graphicsDirty = true;
}

void drawBattleCommandSelection(
  Adafruit_SSD1306& oled
) {
  const uint8_t lastTop =
    BATTLE_COMMAND_COUNT > 2
      ? static_cast<uint8_t>(
          BATTLE_COMMAND_COUNT - 2
        )
      : 0;

  if (
    battleCommandViewportTop >
    lastTop
  ) {
    battleCommandViewportTop =
      lastTop;
  }

  if (
    battleCommandIndex <
    battleCommandViewportTop
  ) {
    battleCommandViewportTop =
      battleCommandIndex;
  } else if (
    battleCommandIndex >=
    battleCommandViewportTop + 2
  ) {
    battleCommandViewportTop =
      battleCommandIndex - 1;
  }

  drawUtf8Text(
    oled,
    0,
    0,
    "행동 선택"
  );

  for (
    uint8_t row = 0;
    row < 2 &&
    battleCommandViewportTop + row <
      BATTLE_COMMAND_COUNT;
    ++row
  ) {
    const uint8_t item =
      battleCommandViewportTop +
      row;

    const int16_t y =
      static_cast<int16_t>(
        18 +
        row * 18
      );

    if (
      item ==
      battleCommandIndex
    ) {
      oled.setCursor(
        0,
        y + 4
      );

      oled.print(
        ">"
      );
    }

    drawUtf8Text(
      oled,
      12,
      y,
      BATTLE_COMMAND_ITEMS[
        item
      ]
    );
  }

  oled.setCursor(
    0,
    56
  );

  oled.print(
    saveError
      ? "SAVE ERROR / OK"
      : "L/R          OK"
  );
}

// 싸운다 하위 메뉴의 표시만 담당한다.
// 저장/행동 메시지와 독립적인 2줄 viewport다.
void drawBattleMoveSelection(
  Adafruit_SSD1306& oled
) {
  using namespace PokemonGame;

  const auto* p =
    partner(
      gameSave.state
    );

  if (!p) {
    return;
  }

  const auto& b =
    gameSave.state.battle;

  selectUsableBattleMove();

  uint8_t slots[4] = {};
  uint8_t count = 0;
  uint8_t selectedRow = 0;

  if (
    battleMoveSlot ==
    STRUGGLE_SLOT
  ) {
    slots[
      count++
    ] =
      STRUGGLE_SLOT;
  } else {
    for (
      uint8_t slot = 0;
      slot < 4;
      ++slot
    ) {
      // PP 소진으로 목록 순서가 바뀌지 않게 기술은 남기고,
      // 입력에서만 건너뛴다.
      if (
        !findMove(
          p->moves[
            slot
          ]
        )
      ) {
        continue;
      }

      if (
        slot ==
        battleMoveSlot
      ) {
        selectedRow =
          count;
      }

      slots[
        count++
      ] =
        slot;
    }
  }

  const uint8_t lastTop =
    count > 2
      ? static_cast<uint8_t>(
          count - 2
        )
      : 0;

  if (
    battleMoveViewportTop >
    lastTop
  ) {
    battleMoveViewportTop =
      lastTop;
  }

  if (
    selectedRow <
    battleMoveViewportTop
  ) {
    battleMoveViewportTop =
      selectedRow;
  } else if (
    selectedRow >=
    battleMoveViewportTop + 2
  ) {
    battleMoveViewportTop =
      selectedRow - 1;
  }

  drawUtf8Text(
    oled,
    0,
    0,
    "기술 선택"
  );

  for (
    uint8_t row = 0;
    row < 2 &&
    battleMoveViewportTop + row <
      count;
    ++row
  ) {
    const uint8_t slot =
      slots[
        battleMoveViewportTop +
        row
      ];

    const auto& move =
      slot == STRUGGLE_SLOT
        ? struggleMove()
        : *findMove(
            p->moves[
              slot
            ]
          );

    const int16_t y =
      static_cast<int16_t>(
        18 +
        row * 18
      );

    if (
      slot ==
      battleMoveSlot
    ) {
      oled.setCursor(
        0,
        y + 4
      );

      oled.print(
        ">"
      );
    }

    drawUtf8Text(
      oled,
      12,
      y,
      move.name
    );
  }

  oled.setCursor(
    0,
    56
  );

  if (
    saveError
  ) {
    oled.print(
      "SAVE ERROR / OK"
    );
  } else if (
    battleMoveSlot ==
    STRUGGLE_SLOT
  ) {
    oled.print(
      "PP --  L/R OK"
    );
  } else {
    char text[32];

    const auto* move =
      findMove(
        p->moves[
          battleMoveSlot
        ]
      );

    snprintf(
      text,
      sizeof(text),
      "PP %u/%u  L/R OK",
      static_cast<unsigned>(
        b.player.pp[
          battleMoveSlot
        ]
      ),
      static_cast<unsigned>(
        move->maxPP
      )
    );

    oled.print(
      text
    );
  }
}

void drawBattleBallSelection(
  Adafruit_SSD1306& oled
) {
  using namespace PokemonGame;

  CaptureBall balls[4] = {};
  const uint8_t ballCount =
    availableBattleBalls(
      balls
    );

  // 마지막 항목은 언제나 "돌아가기".
  const uint8_t optionCount =
    static_cast<uint8_t>(
      ballCount +
      1
    );

  if (
    battleBallIndex >=
    optionCount
  ) {
    battleBallIndex = 0;
  }

  const uint8_t lastTop =
    optionCount > 2
      ? static_cast<uint8_t>(
          optionCount -
          2
        )
      : 0;

  if (
    battleBallViewportTop >
    lastTop
  ) {
    battleBallViewportTop =
      lastTop;
  }

  if (
    battleBallIndex <
    battleBallViewportTop
  ) {
    battleBallViewportTop =
      battleBallIndex;
  } else if (
    battleBallIndex >=
    battleBallViewportTop +
      2
  ) {
    battleBallViewportTop =
      battleBallIndex -
      1;
  }

  drawUtf8Text(
    oled,
    0,
    0,
    "볼 선택"
  );

  for (
    uint8_t row = 0;
    row < 2 &&
    battleBallViewportTop + row <
      optionCount;
    ++row
  ) {
    const uint8_t item =
      static_cast<uint8_t>(
        battleBallViewportTop +
        row
      );

    const int16_t y =
      static_cast<int16_t>(
        18 +
        row * 18
      );

    if (
      item ==
      battleBallIndex
    ) {
      oled.setCursor(
        0,
        y + 4
      );

      oled.print(
        ">"
      );
    }

    drawUtf8Text(
      oled,
      12,
      y,
      item < ballCount
        ? captureBallName(
            balls[item]
          )
        : "돌아가기"
    );
  }

  oled.setCursor(
    0,
    56
  );

  if (
    saveError
  ) {
    oled.print(
      "SAVE ERROR / OK"
    );

    return;
  }

  if (
    battleBallIndex <
      ballCount &&
    balls[
      battleBallIndex
    ] ==
      CaptureBall::Master
  ) {
    char text[24];

    snprintf(
      text,
      sizeof(text),
      "x%u  L/R OK",
      static_cast<unsigned>(
        gameSave.state.progress.masterBallCount
      )
    );

    oled.print(
      text
    );

    return;
  }

  oled.print(
    "L/R          OK"
  );
}

void drawBattleUnavailable(
  Adafruit_SSD1306& oled,
  const char* title
) {
  drawUtf8Text(
    oled,
    0,
    0,
    title
  );

  drawUtf8Text(
    oled,
    0,
    24,
    "아직 준비 중"
  );

  oled.setCursor(
    0,
    56
  );

  oled.print(
    "OK"
  );
}

void drawBattleRunFailed(
  Adafruit_SSD1306& oled
) {
  drawUtf8Text(
    oled,
    0,
    0,
    "도망 실패!"
  );

  drawUtf8Text(
    oled,
    0,
    24,
    "상대가 공격한다"
  );

  oled.setCursor(
    0,
    56
  );

  oled.print(
    "OK"
  );
}

void drawBattleCaptureFailed(
  Adafruit_SSD1306& oled
) {
  drawUtf8Text(
    oled,
    0,
    0,
    "포획 실패!"
  );

  drawUtf8Text(
    oled,
    0,
    24,
    "상대가 공격한다"
  );

  oled.setCursor(
    0,
    56
  );

  oled.print(
    "OK"
  );
}

void drawBattleText(
  Adafruit_SSD1306& oled
) {
  using namespace PokemonGame;

  const auto& b =
    gameSave.state.battle;

  if (
    showingBattleAction()
  ) {
    const auto& action =
      battleReport.actions[
        battleActionIndex
      ];

    const auto* player =
      partner(
        gameSave.state
      );

    const auto* actor =
      action.actor ==
        BattleActor::Player
        ? findSpecies(
            player->speciesId,
            player->formId
          )
        : findSpecies(
            b.wild.speciesId,
            b.wild.formId
          );

    const auto& move =
      action.moveId
        ? *findMove(
            action.moveId
          )
        : struggleMove();

    char text[48];

    snprintf(
      text,
      sizeof(text),
      "%s의",
      actor->name
    );

    drawUtf8Text(
      oled,
      0,
      0,
      text
    );

    snprintf(
      text,
      sizeof(text),
      "%s!",
      move.name
    );

    drawUtf8Text(
      oled,
      0,
      18,
      text
    );

    if (
      !action.hit
    ) {
      snprintf(
        text,
        sizeof(text),
        "빗나갔다!"
      );
    } else if (
      move.category ==
      MoveCategory::Status
    ) {
      snprintf(
        text,
        sizeof(text),
        "변화 기술"
      );
    } else {
      snprintf(
        text,
        sizeof(text),
        "%u 데미지!",
        static_cast<unsigned>(
          action.damage
        )
      );
    }

    drawUtf8Text(
      oled,
      0,
      36,
      text
    );

    oled.setCursor(
      0,
      56
    );

    oled.print(
      "OK"
    );

    return;
  }

  if (
    b.status ==
      BattleStatus::Won ||
    b.status ==
      BattleStatus::Lost
  ) {
    drawUtf8Text(
      oled,
      0,
      0,
      b.status ==
        BattleStatus::Won
        ? "전투 승리!"
        : "쓰러졌다..."
    );

    if (
      saveError
    ) {
      drawUtf8Text(
        oled,
        0,
        24,
        "저장 오류"
      );
    }

    drawUtf8Text(
      oled,
      0,
      48,
      saveError
        ? "OK 재시도"
        : "OK 확인"
    );

    return;
  }

  switch (
    battleUiMode
  ) {
    case BattleUiMode::Command:
      drawBattleCommandSelection(
        oled
      );
      break;

    case BattleUiMode::MoveSelection:
      drawBattleMoveSelection(
        oled
      );
      break;

    case BattleUiMode::BallSelection:
      drawBattleBallSelection(
        oled
      );
      break;

    case BattleUiMode::PartyUnavailable:
      drawBattleUnavailable(
        oled,
        "포켓몬"
      );
      break;

    case BattleUiMode::RunFailed:
      drawBattleRunFailed(
        oled
      );
      break;

    case BattleUiMode::CaptureFailed:
      drawBattleCaptureFailed(
        oled
      );
      break;
    case BattleUiMode::CaptureUnavailable:
      drawUtf8Text(oled, 0, 0, gameSave.state.party.count >= PokemonGame::PARTY_CAPACITY
        ? "파티가 가득 찼다" : "포획 불가");
      drawUtf8Text(oled, 0, 24, gameSave.state.party.count >= PokemonGame::PARTY_CAPACITY
        ? "박스 준비 중" : "개체 ID 부족");
      drawUtf8Text(oled, 0, 48, "OK 확인");
      break;
  }
}

void drawBattleGraphic(
  Adafruit_SSD1306& oled
) {
  using namespace PokemonGame;

  oled.clearDisplay();
  oled.setTextSize(
    1
  );
  oled.setTextColor(
    SSD1306_WHITE
  );

  const auto& b =
    gameSave.state.battle;

  const auto* p =
    partner(
      gameSave.state
    );

  const auto* wild =
    findSpecies(
      b.wild.speciesId,
      b.wild.formId
    );

  if (
    p &&
    wild
  ) {
    char text[24];

    drawUtf8Text(
      oled,
      0,
      0,
      wild->name
    );

    snprintf(
      text,
      sizeof(text),
      "Lv.%u",
      static_cast<unsigned>(
        b.wild.level
      )
    );

    oled.setCursor(
      54,
      4
    );

    oled.print(
      text
    );

    snprintf(
      text,
      sizeof(text),
      "HP %u/%u",
      static_cast<unsigned>(
        b.opponent.currentHp
      ),
      static_cast<unsigned>(
        calculateStats(
          wildPokemon(
            b
          )
        ).hp
      )
    );

    oled.setCursor(
      0,
      20
    );

    oled.print(
      text
    );

    const auto* playerSpecies =
      findSpecies(
        p->speciesId,
        p->formId
      );

    if (
      !playerSpecies
    ) {
      oled.display();
      return;
    }

    drawUtf8Text(
      oled,
      32,
      34,
      playerSpecies->name
    );

    snprintf(
      text,
      sizeof(text),
      "Lv.%u",
      static_cast<unsigned>(
        p->level
      )
    );

    oled.setCursor(
      84,
      38
    );

    oled.print(
      text
    );

    snprintf(
      text,
      sizeof(text),
      "HP %u/%u",
      static_cast<unsigned>(
        b.player.currentHp
      ),
      static_cast<unsigned>(
        calculateStats(
          *p
        ).hp
      )
    );

    oled.setCursor(
      32,
      54
    );

    oled.print(
      text
    );

    const unsigned long elapsed =
      millis() -
      hitEffectStarted;

    const bool blink =
      hitEffect &&
      showingBattleAction() &&
      elapsed < 300 &&
      (
        elapsed / 75
      ) % 2 == 0;

    const bool hideWild =
      blink &&
      battleReport.actions[
        battleActionIndex
      ].actor ==
        BattleActor::Player;

    const bool hidePlayer =
      blink &&
      battleReport.actions[
        battleActionIndex
      ].actor ==
        BattleActor::Wild;

#if HAS_LOCAL_WILD_ASSET
    // 기존 48x48 bitmap을 화면에서만 절반 크기로 그린다.
    // 새 애셋/버퍼는 만들지 않는다.
    auto miniature =
      [&oled](
        SpeciesId id,
        int x,
        int y
      ) {
        const uint8_t* sprite =
          localWildSprite(
            id
          );

        if (
          !sprite
        ) {
          return;
        }

        // 현재 fixture 임시 보정:
        // 꼬렛의 얇은 부분만 2픽셀 임계값으로 보존한다.
        const unsigned threshold =
          id == 19
            ? 2u
            : 3u;

        const int stride =
          (
            WILD_SPRITE_WIDTH +
            7
          ) / 8;

        for (
          int sy = 0;
          sy < WILD_SPRITE_HEIGHT;
          sy += 2
        ) {
          for (
            int sx = 0;
            sx < WILD_SPRITE_WIDTH;
            sx += 2
          ) {
            unsigned ink = 0;

            for (
              int dy = 0;
              dy < 2 &&
              sy + dy <
                WILD_SPRITE_HEIGHT;
              ++dy
            ) {
              for (
                int dx = 0;
                dx < 2 &&
                sx + dx <
                  WILD_SPRITE_WIDTH;
                ++dx
              ) {
                ink +=
                  (
                    pgm_read_byte(
                      sprite +
                      (
                        sy + dy
                      ) *
                      stride +
                      (
                        sx + dx
                      ) /
                      8
                    ) &
                    (
                      0x80u >>
                      (
                        (
                          sx + dx
                        ) %
                        8
                      )
                    )
                  ) != 0
                    ? 1u
                    : 0u;
              }
            }

            if (
              ink >=
              threshold
            ) {
              oled.drawPixel(
                x +
                sx / 2,
                y +
                sy / 2,
                SSD1306_WHITE
              );
            }
          }
        }
      };

    if (
      !hideWild
    ) {
      miniature(
        b.wild.speciesId,
        102,
        0
      );
    }

    if (
      !hidePlayer
    ) {
      miniature(
        p->speciesId,
        2,
        36
      );
    }
#else
    if (
      !hideWild
    ) {
      oled.setCursor(
        110,
        8
      );

      oled.print(
        "?"
      );
    }

    if (
      !hidePlayer
    ) {
      oled.setCursor(
        10,
        44
      );

      oled.print(
        "?"
      );
    }
#endif

    oled.drawLine(
      98,
      26,
      127,
      26,
      SSD1306_WHITE
    );

    oled.drawLine(
      0,
      62,
      29,
      62,
      SSD1306_WHITE
    );
  }

  oled.display();
}

} // namespace

void init() {
  battleReport =
    PokemonGame::BattleTurnReport{};

  battleCaptureReport =
    PokemonGame::BattleCaptureReport{};

  battleActionIndex = 0;
  hitEffect = false;
  captureAnimationStartedAt = 0;
  captureAnimationShakeCount = 0;
  captureAnimationVisualFrame = 0xff;
  resetBattleCommandUi();
  resetBattleBallUi();
  battleMoveSlot = 0;
  battleMoveViewportTop = 0;
  gameMenuIndex = 0;
  partyViewIndex = 0;
  regionChoice = 0;
  regionMessage = nullptr;
  saveError = false;
  graphicsDirty = true;

#if HAS_LOCAL_PIKACHU_ASSET
  pokemonIdleFrame = 0;
  pokemonIdleFrameStartedAt =
    millis();
#endif

  drawDeskPet();
  initGameState();

  completionSaveBlocked = false;

  using namespace PokemonGame;

  if (
    gameSave.state.battle.status !=
    BattleStatus::None
  ) {
    screen =
      GameScreen::Battle;

    selectUsableBattleMove();

    return;
  }

  // 조우가 이미 저장되어 있다면 재부팅 후에도 같은 조우를 바로 보여준다.
  if (
    gameSave.state.encounter.status ==
    EncounterStatus::Ready
  ) {
    screen =
      GameScreen::WildEncounter;

    return;
  }

  if (
    gameSave.state.exploration.status ==
    ExplorationStatus::Exploring
  ) {
    screen =
      GameScreen::Exploring;

    return;
  }

  if (
    gameSave.state.exploration.status ==
    ExplorationStatus::Complete
  ) {
    // G2에서 Complete 상태로 저장된 뒤 G3로 올라온 경우도 여기서 처리한다.
    screen =
      GameScreen::ExplorationComplete;

    if (
      !persistEncounterForCompletedExploration()
    ) {
      completionSaveBlocked =
        true;
    }

    return;
  }

  screen =
    GameScreen::Home;
}

void update() {
  using namespace PokemonGame;

  if (
    gameSave.state.exploration.status !=
      ExplorationStatus::Exploring ||
    completionSaveBlocked
  ) {
    return;
  }

  uint64_t epoch = 0;

  if (
    !NetworkTime::getCurrentEpoch(
      epoch
    )
  ) {
    return;
  }

  GameState next =
    gameSave.state;

  if (
    !updateExploration(
      next.exploration,
      epoch
    )
  ) {
    return;
  }

  // 탐험 완료와 조우 결과를 하나의 GameState에서 확정한다.
  // saveState가 성공해야만 현재 RAM state도 교체된다.
  if (
    !prepareEncounterForCompletedExploration(
      next
    )
  ) {
    saveError = true;
    completionSaveBlocked = true;

    Serial.println(
      "Game encounter resolve failed"
    );

    return;
  }

  if (
    saveState(
      next
    )
  ) {
    screen =
      GameScreen::WildEncounter;

    graphicsDirty = true;
  } else {
    completionSaveBlocked =
      true;
  }
}

void drawDeskPet() {
  drawPokemonGraphic(
    Displays::game()
  );
}

void drawGameGraphics() {
  if (
    screen ==
    GameScreen::CaptureAnimation
  ) {
    drawCaptureAnimationGraphic(
      Displays::desk(),
      false
    );

    graphicsDirty = false;

    return;
  }

  if (
    screen ==
    GameScreen::CaptureSuccess
  ) {
    drawCaptureAnimationGraphic(
      Displays::desk(),
      true
    );

    graphicsDirty = false;

    return;
  }

  if (
    screen ==
    GameScreen::Battle
  ) {
    drawBattleGraphic(
      Displays::desk()
    );

    graphicsDirty = false;

    return;
  }

  if (
    screen ==
    GameScreen::WildEncounter
  ) {
    drawWildEncounterGraphic(
      Displays::desk()
    );
  } else {
    drawPokemonGraphic(
      Displays::desk()
    );
  }

  graphicsDirty = false;
}

void drawGameTextScreen() {
  auto& oled =
    Displays::game();

  oled.clearDisplay();

  oled.setTextColor(
    SSD1306_WHITE
  );

  oled.setTextSize(
    1
  );

  if (
    screen ==
    GameScreen::Battle
  ) {
    drawBattleText(
      oled
    );

    oled.display();

    return;
  }

  if (
    screen ==
    GameScreen::RunSuccess
  ) {
    drawUtf8Text(
      oled,
      0,
      8,
      "도망 성공!"
    );

    drawUtf8Text(
      oled,
      0,
      40,
      "OK 확인"
    );

    oled.display();

    return;
  }

  if (
    screen ==
    GameScreen::CaptureAnimation
  ) {
    drawUtf8Text(
      oled,
      0,
      0,
      captureBallName(
        battleCaptureReport.ball
      )
    );

    drawUtf8Text(
      oled,
      0,
      24,
      "포획 중..."
    );

    oled.display();

    return;
  }

  if (
    screen ==
    GameScreen::CaptureSuccess
  ) {
    const auto* species =
      PokemonGame::findSpecies(
        battleCaptureReport.wild.speciesId,
        battleCaptureReport.wild.formId
      );

    drawUtf8Text(
      oled,
      0,
      0,
      species
        ? species->name
        : "포켓몬"
    );

    drawUtf8Text(
      oled,
      0,
      18,
      "포획 성공!"
    );

    drawUtf8Text(
      oled,
      0,
      36,
      "파티에 합류!"
    );

    oled.setCursor(0, 56);
    oled.print("OK");

    oled.display();

    return;
  }

  if (
    screen ==
    GameScreen::RegionSelect
  ) {
    drawUtf8Text(
      oled,
      0,
      0,
      "지역 선택"
    );

    if (
      regionMessage
    ) {
      drawScrollingUtf8Text(
        oled,
        0,
        24,
        128,
        regionMessage
      );
    } else {
      oled.setCursor(
        0,
        28
      );

      oled.print(
        ">"
      );

      drawUtf8Text(
        oled,
        12,
        24,
        regionChoice == 0
          ? "테스트 초원"
          : "돌아가기"
      );
    }

    drawUtf8Text(
      oled,
      0,
      48,
      "L/R  OK 선택"
    );

    oled.display();

    return;
  }

  if (
    screen ==
    GameScreen::Exploring
  ) {
    drawUtf8Text(
      oled,
      0,
      0,
      "테스트 초원"
    );

    uint64_t epoch = 0;

    if (
      completionSaveBlocked
    ) {
      drawUtf8Text(
        oled,
        0,
        24,
        "저장 오류"
      );

      drawUtf8Text(
        oled,
        0,
        48,
        "OK 재시도"
      );
    } else if (
      !NetworkTime::getCurrentEpoch(
        epoch
      ) ||
      epoch <
        gameSave.state.exploration.startedAtEpoch
    ) {
      drawUtf8Text(
        oled,
        0,
        24,
        "시간 확인 중..."
      );
    } else {
      drawUtf8Text(
        oled,
        0,
        24,
        "탐험 중..."
      );

      const uint32_t remaining =
        PokemonGame::remainingSeconds(
          gameSave.state.exploration,
          epoch
        );

      char remainingText[40];

      if (
        remaining < 60
      ) {
        snprintf(
          remainingText,
          sizeof(remainingText),
          "남은 %lu초",
          static_cast<unsigned long>(
            remaining
          )
        );
      } else {
        snprintf(
          remainingText,
          sizeof(remainingText),
          "남은 %lu:%02lu",
          static_cast<unsigned long>(
            remaining / 60
          ),
          static_cast<unsigned long>(
            remaining % 60
          )
        );
      }

      drawUtf8Text(
        oled,
        0,
        48,
        remainingText
      );
    }

    oled.display();

    return;
  }

  if (
    screen ==
    GameScreen::ExplorationComplete
  ) {
    drawUtf8Text(
      oled,
      0,
      0,
      "탐험 완료!"
    );

    drawUtf8Text(
      oled,
      0,
      24,
      saveError
        ? "저장 오류"
        : "조우 준비 중"
    );

    drawUtf8Text(
      oled,
      0,
      48,
      "OK 재시도"
    );

    oled.display();

    return;
  }

  if (
    screen ==
    GameScreen::WildEncounter
  ) {
    drawWildEncounterText(
      oled
    );

    oled.display();

    return;
  }

  if (
    screen ==
    GameScreen::Party
  ) {
    const auto& party =
      gameSave.state.party;

    if (
      party.count == 0
    ) {
      drawUtf8Text(
        oled,
        0,
        0,
        "파티 없음"
      );

      oled.setCursor(
        0,
        56
      );

      oled.print(
        "OK"
      );

      oled.display();
      return;
    }

    if (
      partyViewIndex >=
      party.count
    ) {
      partyViewIndex = 0;
    }

    const auto& member =
      party.members[
        partyViewIndex
      ];

    const auto* memberSpecies =
      PokemonGame::findSpecies(
        member.speciesId,
        member.formId
      );

    char line[32];

    snprintf(
      line,
      sizeof(line),
      "파티 %u/%u",
      static_cast<unsigned>(
        partyViewIndex + 1
      ),
      static_cast<unsigned>(
        party.count
      )
    );

    drawUtf8Text(
      oled,
      0,
      0,
      line
    );

    drawUtf8Text(
      oled,
      0,
      18,
      memberSpecies
        ? memberSpecies->name
        : "포켓몬"
    );

    const auto memberStats =
      PokemonGame::calculateStats(
        member
      );

    snprintf(
      line,
      sizeof(line),
      "Lv.%u  HP %u/%u",
      static_cast<unsigned>(
        member.level
      ),
      static_cast<unsigned>(
        member.currentHp
      ),
      static_cast<unsigned>(
        memberStats.hp
      )
    );

    oled.setCursor(
      0,
      38
    );

    oled.print(
      line
    );

    oled.setCursor(
      0,
      56
    );

    oled.print(
      party.count > 1
        ? "L/R          OK"
        : "             OK"
    );

    oled.display();
    return;
  }

  const auto* p =
    PokemonGame::partner(
      gameSave.state
    );

  const auto* species =
    p
      ? PokemonGame::findSpecies(
          p->speciesId,
          p->formId
        )
      : nullptr;

  if (
    !p ||
    !species
  ) {
    drawUtf8Text(
      oled,
      0,
      0,
      "파트너 없음"
    );

    oled.display();

    return;
  }

  drawUtf8Text(
    oled,
    0,
    0,
    species->name
  );

  char line[32];

  snprintf(
    line,
    sizeof(line),
    "Lv.%u  HP %u/%u",
    static_cast<unsigned>(
      p->level
    ),
    static_cast<unsigned>(
      p->currentHp
    ),
    static_cast<unsigned>(
      PokemonGame::calculateStats(
        *p
      ).hp
    )
  );

  oled.setCursor(
    0,
    19
  );

  oled.print(
    line
  );

  if (
    screen ==
    GameScreen::Status
  ) {
    oled.setCursor(
      0,
      30
    );

    oled.print(
      "EXP "
    );

    oled.print(
      static_cast<unsigned long>(
        p->exp
      )
    );

    if (
      saveError
    ) {
      drawUtf8Text(
        oled,
        0,
        40,
        "저장 오류"
      );
    } else {
      snprintf(
        line,
        sizeof(line),
        "친밀도 %u",
        static_cast<unsigned>(
          p->friendship
        )
      );

      drawUtf8Text(
        oled,
        0,
        40,
        line
      );
    }

    oled.setCursor(
      0,
      56
    );

    oled.print(
      "OK"
    );
  } else {
    oled.setCursor(
      0,
      saveError
        ? 34
        : 38
    );

    oled.print(
      ">"
    );

    drawUtf8Text(
      oled,
      12,
      saveError
        ? 30
        : 34,
      GAME_MENU_ITEMS[
        gameMenuIndex
      ]
    );

    oled.setCursor(
      0,
      56
    );

    if (
      saveError
    ) {
      drawUtf8Text(
        oled,
        0,
        48,
        "저장 오류"
      );
    } else {
      oled.print(
        "L/R          OK"
      );
    }
  }

  oled.display();
}

void updatePokemonAnimation(
  DeviceMode deviceMode
) {
  if (
    screen ==
    GameScreen::CaptureAnimation
  ) {
    const unsigned long elapsed =
      millis() -
      captureAnimationStartedAt;

    if (
      elapsed >=
      captureAnimationDuration()
    ) {
      finishCaptureAnimation();

      if (
        deviceMode ==
        MODE_GAME
      ) {
        drawGameTextScreen();
      }
    } else {
      const uint8_t frame =
        captureVisualFrameForElapsed(
          elapsed
        );

      if (
        frame !=
        captureAnimationVisualFrame
      ) {
        captureAnimationVisualFrame =
          frame;

        graphicsDirty = true;
      }
    }
  }

  if (
    deviceMode ==
      MODE_GAME &&
    hitEffect
  ) {
    const unsigned long elapsed =
      millis() -
      hitEffectStarted;

    if (
      elapsed >= 300
    ) {
      hitEffect = false;
      graphicsDirty = true;
    } else if (
      elapsed / 75 !=
      hitEffectFrame
    ) {
      hitEffectFrame =
        elapsed / 75;

      graphicsDirty = true;
    }
  }

  if (
    deviceMode ==
      MODE_GAME &&
    graphicsDirty
  ) {
    drawGameGraphics();

    return;
  }

  if (
    deviceMode ==
      MODE_GAME &&
    (
      screen ==
        GameScreen::WildEncounter ||
      screen ==
        GameScreen::Battle ||
      screen ==
        GameScreen::CaptureAnimation ||
      screen ==
        GameScreen::CaptureSuccess
    )
  ) {
    return;
  }

#if HAS_LOCAL_PIKACHU_ASSET
  const unsigned long now =
    millis();

  if (
    now -
      pokemonIdleFrameStartedAt >=
      PIKACHU_IDLE_FRAME_DURATION_MS[
        pokemonIdleFrame
      ]
  ) {
    pokemonIdleFrame =
      (
        pokemonIdleFrame +
        1
      ) %
      PIKACHU_IDLE_FRAME_COUNT;

    pokemonIdleFrameStartedAt =
      now;

    if (
      deviceMode ==
      MODE_DESK
    ) {
      drawDeskPet();
    } else {
      drawGameGraphics();
    }
  }
#endif
}

void handleButton(
  ButtonEvent button
) {
  if (
    screen ==
    GameScreen::CaptureAnimation
  ) {
    // 판정/저장은 이미 끝난 상태다. 연출 중 입력은 결과를 건너뛰지 않는다.
    drawGameTextScreen();

    return;
  }

  if (
    screen ==
    GameScreen::RunSuccess
  ) {
    if (
      button ==
      BUTTON_OK
    ) {
      screen =
        GameScreen::Home;

      graphicsDirty = true;
    }

    drawGameTextScreen();

    return;
  }

  if (
    screen ==
    GameScreen::CaptureSuccess
  ) {
    if (
      button ==
      BUTTON_OK
    ) {
      screen =
        GameScreen::Home;

      battleCaptureReport =
        PokemonGame::BattleCaptureReport{};

      graphicsDirty = true;
    }

    drawGameTextScreen();

    return;
  }

  if (
    screen ==
    GameScreen::Battle
  ) {
    using namespace PokemonGame;

    const auto* p =
      partner(
        gameSave.state
      );

    if (
      !p
    ) {
      return;
    }

    if (
      showingBattleAction()
    ) {
      if (
        button ==
        BUTTON_OK
      ) {
        ++battleActionIndex;

        beginActionFeedback();

        if (
          !showingBattleAction() &&
          gameSave.state.battle.status ==
            BattleStatus::Active
        ) {
          resetBattleCommandUi();
        }
      }

      drawGameTextScreen();

      return;
    }

    if (
      gameSave.state.battle.status ==
      BattleStatus::Active
    ) {
      switch (
        battleUiMode
      ) {
        case BattleUiMode::Command:
          if (
            button ==
            BUTTON_LEFT
          ) {
            battleCommandIndex =
              static_cast<uint8_t>(
                (
                  battleCommandIndex +
                  BATTLE_COMMAND_COUNT -
                  1
                ) %
                BATTLE_COMMAND_COUNT
              );
          } else if (
            button ==
            BUTTON_RIGHT
          ) {
            battleCommandIndex =
              static_cast<uint8_t>(
                (
                  battleCommandIndex +
                  1
                ) %
                BATTLE_COMMAND_COUNT
              );
          } else if (
            button ==
            BUTTON_OK
          ) {
            switch (
              battleCommandIndex
            ) {
              case 0:
                battleUiMode =
                  BattleUiMode::MoveSelection;

                selectUsableBattleMove();

                break;

              case 1:
                resetBattleBallUi();

                battleUiMode =
                  BattleUiMode::BallSelection;

                break;

              case 2:
                battleUiMode =
                  BattleUiMode::PartyUnavailable;

                break;

              case 3: {
                auto next =
                  gameSave.state;

                BattleRunReport runReport;

                if (
                  attemptBattleRun(
                    next,
                    &runReport
                  ) &&
                  saveState(
                    next
                  )
                ) {
                  if (
                    runReport.escaped
                  ) {
                    battleReport =
                      BattleTurnReport{};

                    battleActionIndex = 0;
                    hitEffect = false;

                    // BattleState는 이미 None으로 저장됐지만,
                    // 성공 메시지는 일시 UI로 한 번 보여준 뒤 HOME으로 간다.
                    screen =
                      GameScreen::RunSuccess;

                    resetBattleCommandUi();

                    graphicsDirty = true;
                  } else {
                    battleReport =
                      BattleTurnReport{};

                    if (
                      runReport.opponentActed
                    ) {
                      battleReport.actions[0] =
                        runReport.opponentAction;

                      battleReport.count = 1;
                    }

                    // 먼저 "도망 실패"를 보여주고,
                    // OK를 누르면 저장된 상대 행동 결과를 표시한다.
                    battleActionIndex =
                      battleReport.count;

                    hitEffect = false;

                    battleUiMode =
                      BattleUiMode::RunFailed;

                    graphicsDirty = true;
                  }
                }

                break;
              }

              default:
                break;
            }
          }

          break;

        case BattleUiMode::MoveSelection:
          if (
            button ==
              BUTTON_LEFT ||
            button ==
              BUTTON_RIGHT
          ) {
            battleMoveSlot =
              nextBattleMove(
                *p,
                gameSave.state.battle,
                battleMoveSlot,
                button ==
                  BUTTON_LEFT
                  ? -1
                  : 1
              );
          } else if (
            button ==
            BUTTON_OK
          ) {
            auto next =
              gameSave.state;

            BattleTurnReport report;

            if (
              resolveBattleTurn(
                next,
                battleMoveSlot,
                &report
              ) &&
              saveState(
                next
              )
            ) {
              battleReport =
                report;

              battleActionIndex = 0;

              selectUsableBattleMove();

              // 행동 메시지가 끝난 뒤에는 다시 전투 명령 메뉴로 돌아간다.
              resetBattleCommandUi();

              beginActionFeedback();
            }
          }

          break;

        case BattleUiMode::BallSelection: {
          CaptureBall balls[4] = {};
          const uint8_t ballCount =
            availableBattleBalls(
              balls
            );

          const uint8_t optionCount =
            static_cast<uint8_t>(
              ballCount +
              1
            );

          if (
            button ==
            BUTTON_LEFT
          ) {
            battleBallIndex =
              static_cast<uint8_t>(
                (
                  battleBallIndex +
                  optionCount -
                  1
                ) %
                optionCount
              );
          } else if (
            button ==
            BUTTON_RIGHT
          ) {
            battleBallIndex =
              static_cast<uint8_t>(
                (
                  battleBallIndex +
                  1
                ) %
                optionCount
              );
          } else if (
            button ==
            BUTTON_OK
          ) {
            if (
              battleBallIndex >=
              ballCount
            ) {
              resetBattleCommandUi();
              resetBattleBallUi();
            } else if (gameSave.state.party.count >= PARTY_CAPACITY ||
                       gameSave.state.progress.nextInstanceId == UINT32_MAX) {
              // 포획 시도/저장/연출 없이 안내만 표시한다.
              battleUiMode = BattleUiMode::CaptureUnavailable;
            } else {
              auto next =
                gameSave.state;

              BattleCaptureReport captureReport;

              if (
                attemptBattleCapture(
                  next,
                  balls[
                    battleBallIndex
                  ],
                  &captureReport
                ) &&
                saveState(
                  next
                )
              ) {
                battleCaptureReport =
                  captureReport;

                battleReport =
                  BattleTurnReport{};

                battleActionIndex = 0;
                hitEffect = false;

                if (
                  captureReport.opponentActed
                ) {
                  battleReport.actions[0] =
                    captureReport.opponentAction;

                  battleReport.count = 1;
                }

                // G5-B.1:
                // 포획 판정과 실패 반격까지 저장된 뒤에만 transient 흔들림 연출을 시작한다.
                // 성공은 3회, 실패는 1~2회 흔든 뒤 기존 결과 UI로 이어진다.
                if (
                  captureReport.captured
                ) {
                  resetBattleCommandUi();
                  resetBattleBallUi();
                }

                beginCaptureAnimation();
              }
            }
          }

          break;
        }

        case BattleUiMode::CaptureUnavailable:
          if (button == BUTTON_OK) battleUiMode = BattleUiMode::BallSelection;
          break;

        case BattleUiMode::PartyUnavailable:
          if (
            button ==
            BUTTON_OK
          ) {
            resetBattleCommandUi();
          }

          break;

        case BattleUiMode::RunFailed:
          if (
            button ==
            BUTTON_OK
          ) {
            if (
              battleReport.count > 0
            ) {
              battleActionIndex = 0;
              beginActionFeedback();
            } else {
              resetBattleCommandUi();
            }
          }

          break;
        case BattleUiMode::CaptureFailed:
          if (
            button ==
            BUTTON_OK
          ) {
            if (
              battleReport.count > 0
            ) {
              battleActionIndex = 0;
              beginActionFeedback();
            } else {
              resetBattleCommandUi();
            }
          }

          break;
      }
    } else if (
      button ==
      BUTTON_OK
    ) {
      auto next =
        gameSave.state;

      if (
        acknowledgeBattle(
          next
        ) &&
        saveState(
          next
        )
      ) {
        screen =
          GameScreen::Home;

        resetBattleCommandUi();

        graphicsDirty = true;
      }
    }

    drawGameTextScreen();

    return;
  }

  if (
    screen ==
    GameScreen::RegionSelect
  ) {
    if (
      button ==
        BUTTON_LEFT ||
      button ==
        BUTTON_RIGHT
    ) {
      regionChoice =
        (
          regionChoice +
          1
        ) %
        2;

      regionMessage =
        nullptr;
    } else if (
      button ==
      BUTTON_OK
    ) {
      if (
        regionChoice ==
        1
      ) {
        screen =
          GameScreen::Home;

        regionMessage =
          nullptr;
      } else {
        uint64_t epoch = 0;

        if (
          !NetworkTime::getCurrentEpoch(
            epoch
          )
        ) {
          regionMessage =
            "시간 동기화 중...";
        } else {
          auto next =
            gameSave.state;

          if (
            next.encounter.status !=
            PokemonGame::EncounterStatus::None
          ) {
            regionMessage =
              "조우 확인 필요";
          } else if (
            PokemonGame::startExploration(
              next.exploration,
              PokemonGame::TEST_REGION_ID,
              epoch,
              PokemonGame::TEST_EXPLORATION_SECONDS
            ) &&
            saveState(
              next
            )
          ) {
            screen =
              GameScreen::Exploring;

            completionSaveBlocked =
              false;

            regionMessage =
              nullptr;
          } else {
            regionMessage =
              "저장 오류";
          }
        }
      }
    }

    drawGameTextScreen();

    return;
  }

  if (
    screen ==
    GameScreen::Exploring
  ) {
    if (
      button ==
        BUTTON_OK &&
      completionSaveBlocked
    ) {
      completionSaveBlocked =
        false;

      update();

      drawGameTextScreen();
    }

    return;
  }

  if (
    screen ==
    GameScreen::ExplorationComplete
  ) {
    if (
      button ==
      BUTTON_OK
    ) {
      completionSaveBlocked =
        false;

      if (
        !persistEncounterForCompletedExploration()
      ) {
        completionSaveBlocked =
          true;
      }

      drawGameTextScreen();
    }

    return;
  }

  if (
    screen ==
    GameScreen::WildEncounter
  ) {
    if (
      button ==
      BUTTON_OK
    ) {
      auto next =
        gameSave.state;

      if (
        PokemonGame::startBattle(
          next
        ) &&
        saveState(
          next
        )
      ) {
        screen =
          GameScreen::Battle;

        battleReport =
          PokemonGame::BattleTurnReport{};

        battleActionIndex = 0;
        hitEffect = false;
        battleMoveSlot = 0;
        battleMoveViewportTop = 0;

        resetBattleCommandUi();

        selectUsableBattleMove();

        graphicsDirty = true;
      }

      drawGameTextScreen();
    }

    return;
  }

  if (
    screen ==
    GameScreen::Party
  ) {
    const uint8_t count =
      gameSave.state.party.count;

    if (
      count > 0 &&
      (
        button ==
          BUTTON_LEFT ||
        button ==
          BUTTON_RIGHT
      )
    ) {
      if (
        button ==
        BUTTON_LEFT
      ) {
        partyViewIndex =
          static_cast<uint8_t>(
            (
              partyViewIndex +
              count -
              1
            ) %
            count
          );
      } else {
        partyViewIndex =
          static_cast<uint8_t>(
            (
              partyViewIndex +
              1
            ) %
            count
          );
      }
    } else if (
      button ==
      BUTTON_OK
    ) {
      screen =
        GameScreen::Home;
    }

    drawGameTextScreen();
    return;
  }

  if (
    screen ==
    GameScreen::Status
  ) {
    if (
      button ==
      BUTTON_OK
    ) {
      screen =
        GameScreen::Home;

      drawGameTextScreen();
    }

    return;
  }

  if (
    button ==
    BUTTON_LEFT
  ) {
    gameMenuIndex =
      (
        gameMenuIndex -
        1 +
        GAME_MENU_ITEM_COUNT
      ) %
      GAME_MENU_ITEM_COUNT;

    drawGameTextScreen();

    return;
  }

  if (
    button ==
    BUTTON_RIGHT
  ) {
    gameMenuIndex =
      (
        gameMenuIndex +
        1
      ) %
      GAME_MENU_ITEM_COUNT;

    drawGameTextScreen();

    return;
  }

  if (
    button ==
    BUTTON_OK
  ) {
    Serial.print(
      "Game menu selected: "
    );

    Serial.println(
      GAME_MENU_ITEMS[
        gameMenuIndex
      ]
    );

    if (
      gameMenuIndex ==
      0
    ) {
      screen =
        GameScreen::Status;

      drawGameTextScreen();
    } else if (
      gameMenuIndex ==
      1
    ) {
      partyViewIndex = 0;
      screen =
        GameScreen::Party;

      drawGameTextScreen();
    } else if (
      gameMenuIndex ==
      2
    ) {
      regionChoice = 0;
      regionMessage = nullptr;
      screen =
        GameScreen::RegionSelect;

      drawGameTextScreen();
    }

    return;
  }
}

const PokemonGame::GameState& state() {
  return gameSave.state;
}

bool saveState(
  const PokemonGame::GameState& next
) {
  PokemonGame::GameSave candidate =
    gameSave;

  candidate.state =
    next;

  if (
    !GameSaveStorage::save(
      candidate
    )
  ) {
    saveError = true;

    Serial.println(
      "Game save failed; current state kept"
    );

    return false;
  }

  gameSave =
    candidate;

  saveError = false;

  Serial.println(
    "Game save succeeded"
  );

  return true;
}

} // namespace GameApp
