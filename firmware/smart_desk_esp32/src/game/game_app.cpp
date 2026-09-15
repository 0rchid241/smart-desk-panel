#include "game_app.h"
#include "save_storage.h"
#include "../hardware/displays.h"
#include "../core/app_config.h"
#include "../services/network_time.h"
#include "../../hangul_renderer.h"
// 로컬 전용 포켓몬 애셋.
// 이 파일은 Git에 올리지 않아도 공개 저장소가 컴파일되도록 조건부 포함한다.
#if __has_include("../../local_game_assets/pokemon/pikachu_idle_1bit.h")
  #include "../../local_game_assets/pokemon/pikachu_idle_1bit.h"
  #define HAS_LOCAL_PIKACHU_ASSET 1
#else
  #define HAS_LOCAL_PIKACHU_ASSET 0
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

int pokemonIdleFrame =
  0;

unsigned long pokemonIdleFrameStartedAt =
  0;

#endif


const char* GAME_MENU_ITEMS[] = {
  "상태",
  "탐험",
  "도감"
};

const int GAME_MENU_ITEM_COUNT =
  sizeof(GAME_MENU_ITEMS) /
  sizeof(GAME_MENU_ITEMS[0]);

int gameMenuIndex =
  0;

PokemonGame::GameSave gameSave;
enum class GameScreen { Home, Status, RegionSelect, Exploring, ExplorationComplete };
GameScreen screen = GameScreen::Home;
bool saveError = false;
// Stop automatic flash retries after a failure; OK explicitly retries completion.
bool completionSaveBlocked = false;
int regionChoice = 0; // 0: the public fixture, 1: return Home (not a second region).
const char* regionMessage = nullptr;

void initGameState() {
  using GameSaveStorage::LoadResult;
  const auto result = GameSaveStorage::load(gameSave);
  if (result == LoadResult::Loaded) {
    Serial.println("Game save loaded");
    if (gameSave.saveVersion != PokemonGame::SAVE_VERSION) {
      // Keep the loaded G1 state even if the migration write fails.
      saveError = !GameSaveStorage::save(gameSave);
      Serial.println(saveError ? "Game migration save failed; G1 state kept" : "Game save migrated to G2");
    }
    return;
  }
  gameSave = PokemonGame::GameSave{};
  gameSave.state = PokemonGame::createNewGame();
  if (result == LoadResult::Missing || result == LoadResult::Invalid) {
    Serial.println(result == LoadResult::Missing ? "Game: first save" : "Game: invalid save fallback");
    saveError = !GameSaveStorage::save(gameSave);
  } else {
    // Storage errors/newer versions keep their original bytes; use RAM fallback.
    saveError = true;
    Serial.println(result == LoadResult::UnsupportedVersion ?
                   "Game: unsupported save version" : "Game: storage unavailable");
  }
  Serial.println(saveError ? "Game save failed (RAM only)" : "Game save created");
}

void drawPokemonGraphic(
  Adafruit_SSD1306& target
);

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

  target.setTextSize(1);

  target.setCursor(
    0,
    0
  );

  drawUtf8Text(target, 0, 0, "포켓몬");

  target.drawLine(
    0,
    18,
    127,
    18,
    SSD1306_WHITE
  );

  target.setCursor(
    12,
    27
  );

  drawUtf8Text(target, 12, 26, "로컬 그림");

  target.setCursor(
    21,
    40
  );

  drawUtf8Text(target, 21, 46, "없음");

#endif

  target.display();
}
} // namespace

void init() {
  gameMenuIndex = 0;
  regionChoice = 0;
  regionMessage = nullptr;
  saveError = false;
#if HAS_LOCAL_PIKACHU_ASSET
  pokemonIdleFrame = 0;
  pokemonIdleFrameStartedAt =
    millis();
#endif

  drawDeskPet();

  initGameState();
  completionSaveBlocked = false;
  if (gameSave.state.exploration.status == PokemonGame::ExplorationStatus::Exploring)
    screen = GameScreen::Exploring;
  else if (gameSave.state.exploration.status == PokemonGame::ExplorationStatus::Complete)
    screen = GameScreen::ExplorationComplete;
  else
    screen = GameScreen::Home;

}

void update() {
  using namespace PokemonGame;
  if (gameSave.state.exploration.status != ExplorationStatus::Exploring || completionSaveBlocked)
    return;
  uint64_t epoch = 0;
  if (!NetworkTime::getCurrentEpoch(epoch)) return;
  GameState next = gameSave.state;
  if (!updateExploration(next.exploration, epoch)) return;
  if (saveState(next)) {
    screen = GameScreen::ExplorationComplete;
  } else {
    completionSaveBlocked = true;
  }
}

void drawDeskPet() {

  drawPokemonGraphic(
    Displays::game()
  );
}

void drawGameGraphics() {

  drawPokemonGraphic(
    Displays::desk()
  );
}

void drawGameTextScreen() {
  auto& oled = Displays::game();

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);

  // Exploration views use the existing 16px Hangul renderer at three row heights.
  if (screen == GameScreen::RegionSelect) {
    drawUtf8Text(oled, 0, 0, "지역 선택");
    if (regionMessage) {
      drawScrollingUtf8Text(oled, 0, 24, 128, regionMessage);
    } else {
      oled.setCursor(0, 28);
      oled.print(">");
      drawUtf8Text(oled, 12, 24, regionChoice == 0 ? "테스트 초원" : "돌아가기");
    }
    drawUtf8Text(oled, 0, 48, "L/R  OK 선택");
    oled.display();
    return;
  }
  if (screen == GameScreen::Exploring) {
    drawUtf8Text(oled, 0, 0, "테스트 초원");
    uint64_t epoch = 0;
    if (completionSaveBlocked) {
      drawUtf8Text(oled, 0, 24, "저장 오류");
      drawUtf8Text(oled, 0, 48, "OK 재시도");
    } else if (!NetworkTime::getCurrentEpoch(epoch) ||
               epoch < gameSave.state.exploration.startedAtEpoch) {
      drawUtf8Text(oled, 0, 24, "시간 확인 중...");
    } else {
      drawUtf8Text(oled, 0, 24, "탐험 중...");
      const uint32_t remaining = PokemonGame::remainingSeconds(gameSave.state.exploration, epoch);
      char remainingText[40];
      if (remaining < 60)
        snprintf(remainingText, sizeof(remainingText), "남은 %lu초", static_cast<unsigned long>(remaining));
      else
        snprintf(remainingText, sizeof(remainingText), "남은 %lu:%02lu",
                 static_cast<unsigned long>(remaining / 60), static_cast<unsigned long>(remaining % 60));
      drawUtf8Text(oled, 0, 48, remainingText);
    }
    oled.display();
    return;
  }
  if (screen == GameScreen::ExplorationComplete) {
    drawUtf8Text(oled, 0, 0, "탐험 완료!");
    drawUtf8Text(oled, 0, 24, saveError ? "저장 오류" : "탐험을 마쳤다.");
    drawUtf8Text(oled, 0, 48, saveError ? "OK 재시도" : "OK 확인");
    oled.display();
    return;
  }

  const auto* p =
    PokemonGame::partner(gameSave.state);

  const auto* species =
    p
      ? PokemonGame::findSpecies(
          p->speciesId,
          p->formId
        )
      : nullptr;


  // 파트너를 찾지 못한 경우
  if (!p || !species) {
    drawUtf8Text(
      oled,
      0,
      0,
      "파트너 없음"
    );

    oled.display();
    return;
  }


  // 포켓몬 이름
  drawUtf8Text(
    oled,
    0,
    0,
    species->name
  );


  // Lv / HP
  char line[32];

  snprintf(
    line,
    sizeof(line),
    "Lv.%u  HP %u/%u",
    static_cast<unsigned>(p->level),
    static_cast<unsigned>(p->currentHp),
    static_cast<unsigned>(
      PokemonGame::calculateStats(*p).hp
    )
  );

  oled.setCursor(
    0,
    19
  );

  oled.print(line);


  // 상태 화면
  if (screen == GameScreen::Status) {

    oled.setCursor(
      0,
      30
    );

    oled.print("EXP ");

    oled.print(
      static_cast<unsigned long>(
        p->exp
      )
    );


    if (saveError) {

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


    // OK = 뒤로가기
    oled.setCursor(
      0,
      56
    );

    oled.print("OK");

  }

  // 게임 메인 메뉴
  else {

    oled.setCursor(
      0,
      saveError ? 34 : 38
    );

    oled.print(">");


    drawUtf8Text(
      oled,
      12,
      saveError ? 30 : 34,
      GAME_MENU_ITEMS[
        gameMenuIndex
      ]
    );


    oled.setCursor(
      0,
      56
    );

    if (saveError) {
      // Draw above the bottom edge to preserve the full 16px glyph height.
      drawUtf8Text(oled, 0, 48, "저장 오류");
    } else {
      oled.print(
        "L/R          OK"
      );
    }
  }


  oled.display();
}

void updatePokemonAnimation(DeviceMode deviceMode) {

#if HAS_LOCAL_PIKACHU_ASSET

  unsigned long now =
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
        pokemonIdleFrame + 1
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

void handleButton(ButtonEvent button) {
    if (screen == GameScreen::RegionSelect) {
      if (button == BUTTON_LEFT || button == BUTTON_RIGHT) {
        regionChoice = (regionChoice + 1) % 2;
        regionMessage = nullptr;
      } else if (button == BUTTON_OK) {
        if (regionChoice == 1) {
          screen = GameScreen::Home;
          regionMessage = nullptr;
        } else {
          uint64_t epoch = 0;
          if (!NetworkTime::getCurrentEpoch(epoch)) {
            regionMessage = "시간 동기화 중...";
          } else {
            auto next = gameSave.state;
            if (PokemonGame::startExploration(next.exploration, PokemonGame::TEST_REGION_ID,
                                             epoch, PokemonGame::TEST_EXPLORATION_SECONDS) && saveState(next)) {
              screen = GameScreen::Exploring;
              completionSaveBlocked = false;
              regionMessage = nullptr;
            } else {
              regionMessage = "저장 오류";
            }
          }
        }
      }
      drawGameTextScreen();
      return;
    }
    if (screen == GameScreen::Exploring) {
      if (button == BUTTON_OK && completionSaveBlocked) {
        completionSaveBlocked = false;
        update();
        drawGameTextScreen();
      }
      return;
    }
    if (screen == GameScreen::ExplorationComplete) {
      if (button == BUTTON_OK) {
        auto next = gameSave.state;
        if (PokemonGame::acknowledgeExploration(next.exploration) && saveState(next))
          screen = GameScreen::Home;
        drawGameTextScreen();
      }
      return;
    }
    if (screen == GameScreen::Status) {
      if (button == BUTTON_OK) {
        screen = GameScreen::Home;
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

      if (gameMenuIndex == 0) {
        screen = GameScreen::Status;
        drawGameTextScreen();
      } else if (gameMenuIndex == 1) {
        regionChoice = 0;
        regionMessage = nullptr;
        screen = GameScreen::RegionSelect;
        drawGameTextScreen();
      }

      return;
    }


    return;
}

const PokemonGame::GameState& state() { return gameSave.state; }

bool saveState(const PokemonGame::GameState& next) {
  PokemonGame::GameSave candidate = gameSave;
  candidate.state = next;
  if (!GameSaveStorage::save(candidate)) {
    saveError = true;
    Serial.println("Game save failed; current state kept");
    return false;
  }
  gameSave = candidate;
  saveError = false;
  Serial.println("Game save succeeded");
  return true;
}
} // namespace GameApp
