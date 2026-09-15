#include "game_app.h"
#include "save_storage.h"
#include "../hardware/displays.h"
#include "../core/app_config.h"
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
  "STATUS",
  "EXPLORE",
  "POKEDEX"
};

const int GAME_MENU_ITEM_COUNT =
  sizeof(GAME_MENU_ITEMS) /
  sizeof(GAME_MENU_ITEMS[0]);

int gameMenuIndex =
  0;

PokemonGame::GameSave gameSave;
bool statusOpen = false;
bool saveError = false;

void initGameState() {
  using GameSaveStorage::LoadResult;
  const auto result = GameSaveStorage::load(gameSave);
  if (result == LoadResult::Loaded) {
    Serial.println("Game save loaded");
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

  target.println(
    "POKEMON"
  );

  target.drawLine(
    0,
    12,
    127,
    12,
    SSD1306_WHITE
  );

  target.setCursor(
    12,
    27
  );

  target.println(
    "LOCAL ASSET"
  );

  target.setCursor(
    21,
    40
  );

  target.println(
    "NOT FOUND"
  );

#endif

  target.display();
}
} // namespace

void init() {
#if HAS_LOCAL_PIKACHU_ASSET
  pokemonIdleFrame = 0;
  pokemonIdleFrameStartedAt =
    millis();
#endif

  drawDeskPet();

  initGameState();

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
  const auto* p = PokemonGame::partner(gameSave.state);
  const auto* species = p ? PokemonGame::findSpecies(p->speciesId, p->formId) : nullptr;
  oled.setCursor(0, 0);
  oled.print(species ? species->name : "NO PARTNER");
  oled.drawLine(0, 11, 127, 11, SSD1306_WHITE);
  if (p && species) {
    char line[24];
    snprintf(line, sizeof(line), "Lv.%u HP %u/%u", static_cast<unsigned>(p->level),
             static_cast<unsigned>(p->currentHp),
             static_cast<unsigned>(PokemonGame::calculateStats(*p).hp));
    oled.setCursor(0, 17);
    oled.print(line);
    if (statusOpen) {
      oled.setCursor(0, 30);
      oled.print("EXP ");
      oled.print(static_cast<unsigned long>(p->exp));
      oled.setCursor(0, 43);
      oled.print("Friendship ");
      oled.print(p->friendship);
    }
  }
  if (!statusOpen) {
    oled.setCursor(0, 30);
    oled.print("> ");
    oled.print(GAME_MENU_ITEMS[gameMenuIndex]);
    oled.setCursor(0, 45);
    oled.print("L/R SELECT");
  }
  oled.setCursor(0, 56);
  oled.print(saveError ? (statusOpen ? "OK BACK / SAVE ERROR" : "OK ENTER / SAVE ERROR")
                       : (statusOpen ? "OK BACK" : "OK ENTER"));
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
    if (statusOpen) {
      if (button == BUTTON_OK) {
        statusOpen = false;
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
        statusOpen = true;
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
