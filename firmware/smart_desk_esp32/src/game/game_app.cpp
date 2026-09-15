#include "game_app.h"
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

  Displays::game().clearDisplay();

  Displays::game().setTextColor(
    SSD1306_WHITE
  );

  Displays::game().setTextSize(1);

  Displays::game().setCursor(
    0,
    0
  );

  Displays::game().print(
    "POKEMON GAME"
  );

  Displays::game().drawLine(
    0,
    11,
    127,
    11,
    SSD1306_WHITE
  );

  Displays::game().setCursor(
    0,
    17
  );

  Displays::game().print(
    "Pikachu"
  );

  Displays::game().setCursor(
    0,
    30
  );

  Displays::game().print(
    "> "
  );

  Displays::game().print(
    GAME_MENU_ITEMS[
      gameMenuIndex
    ]
  );

  Displays::game().setCursor(
    0,
    45
  );

  Displays::game().print(
    "L/R SELECT"
  );

  Displays::game().setCursor(
    0,
    56
  );

  Displays::game().print(
    "OK ENTER"
  );

  Displays::game().display();
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

      return;
    }


    return;
}
} // namespace GameApp
