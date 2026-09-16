#include "game_app.h"
#include "save_storage.h"
#include "Preferences.h"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#include <cassert>
#include <cstdio>

namespace {
Adafruit_SSD1306 desk;
Adafruit_SSD1306 game;

bool gameVisible(
  const char* text
) {
  return
    game.text.find(
      text
    ) !=
    std::string::npos;
}

void enterFight() {
  assert(
    gameVisible(
      "행동 선택"
    )
  );

  GameApp::handleButton(
    BUTTON_OK
  );

  assert(
    gameVisible(
      "기술 선택"
    )
  );
}

} // namespace

namespace Displays {

Adafruit_SSD1306& desk() {
  return ::desk;
}

Adafruit_SSD1306& game() {
  return ::game;
}

} // namespace Displays

namespace NetworkTime {

bool getCurrentEpoch(
  uint64_t& epoch
) {
  epoch = 0;
  return false;
}

} // namespace NetworkTime

void drawUtf8Text(
  Adafruit_SSD1306& oled,
  int16_t x,
  int16_t y,
  const char* text
) {
  int width = 0;

  for (
    const auto* p =
      reinterpret_cast<const unsigned char*>(
        text
      );
    *p;
    ++p
  ) {
    width +=
      *p < 128
        ? 6
        : (
            (*p & 0xc0) ==
              0xc0
              ? 16
              : 0
          );
  }

  assert(
    x >= 0 &&
    x + width <= 128 &&
    y >= 0 &&
    y + 16 <= 64
  );

  oled.text += text;
}

void drawScrollingUtf8Text(
  Adafruit_SSD1306& oled,
  int16_t x,
  int16_t y,
  int16_t,
  const char* text
) {
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

  assert(
    game.bitmaps > 0
  );

  for (
    SpeciesId id :
    {
      SpeciesId(
        16
      ),
      SpeciesId(
        19
      ),
      SpeciesId(
        25
      )
    }
  ) {
    auto next =
      createNewGame();

    assert(
      setEncounter(
        next.encounter,
        id,
        0,
        100,
        Gender::Female,
        true
      )
    );

    assert(
      GameApp::saveState(
        next
      )
    );

    GameApp::init();

    const auto oldBitmaps =
      desk.bitmaps;

    GameApp::drawGameGraphics();

    assert(
      desk.bitmaps ==
      oldBitmaps + 1
    );

    // 조우와 전투 상태 모두 DESK 펫 애니메이션이 계속 움직인다.
    auto checkDesk =
      []() {
        const auto old =
          game.bitmaps;

        hostMillis +=
          2100;

        GameApp::updatePokemonAnimation(
          MODE_DESK
        );

        assert(
          game.bitmaps ==
          old + 1
        );
      };

    checkDesk();

    // Encounter -> Battle.
    GameApp::handleButton(
      BUTTON_OK
    );

    assert(
      GameApp::state().battle.status ==
      BattleStatus::Active
    );

    GameApp::drawGameTextScreen();

    assert(
      gameVisible(
        "행동 선택"
      ) &&
      gameVisible(
        "싸운다"
      )
    );

    const unsigned wildPixels =
      id == 19
        ? 3u
        : 2u;

    const auto pixels =
      desk.pixels;

    GameApp::drawGameGraphics();

    assert(
      desk.pixels ==
      pixels +
      wildPixels +
      2
    );

    assert(
      desk.text.find(
        "WILD"
      ) ==
        std::string::npos &&
      desk.text.find(
        "YOU"
      ) ==
        std::string::npos
    );

    assert(
      desk.text.find(
        "Lv.100"
      ) !=
        std::string::npos &&
      desk.text.find(
        "HP"
      ) !=
        std::string::npos
    );

    checkDesk();

    const auto writes =
      FakeNvs::writes;

    const auto draws =
      desk.draws;

    hostMillis +=
      2100;

    GameApp::updatePokemonAnimation(
      MODE_GAME
    );

    GameApp::update();

    assert(
      desk.draws ==
      draws &&
      FakeNvs::writes ==
      writes
    );

    assert(
      GameApp::state().battle.turn ==
      0
    );

    // 위 checkDesk()는 실제 DESK 모드처럼 text OLED에 Desk Pet을 그린다.
    // GAME 복귀 시 sketch가 text 화면을 다시 그리는 동작을 host test에서도 재현한다.
    GameApp::drawGameTextScreen();

    // G5-A1: 싸운다를 선택한 뒤에만 턴이 실행된다.
    enterFight();

    // 상대 선공 피격: 300ms 동안 75ms 간격 blink,
    // 시간 경과는 저장하지 않는다.
    GameApp::handleButton(
      BUTTON_OK
    );

    const auto feedbackWrites =
      FakeNvs::writes;

    auto beforePixels =
      desk.pixels;

    GameApp::updatePokemonAnimation(
      MODE_GAME
    );

    assert(
      desk.pixels ==
      beforePixels +
      wildPixels
    );

    for (
      const auto& pixel :
      desk.framePixels
    ) {
      assert(
        pixel.first >=
        102
      );
    }

    beforePixels =
      desk.pixels;

    hostMillis +=
      75;

    GameApp::updatePokemonAnimation(
      MODE_GAME
    );

    assert(
      desk.pixels ==
      beforePixels +
      wildPixels +
      2
    );

    const auto deskDraws =
      desk.draws;

    hostMillis +=
      75;

    GameApp::updatePokemonAnimation(
      MODE_DESK
    );

    assert(
      desk.draws ==
      deskDraws
    );

    beforePixels =
      desk.pixels;

    GameApp::updatePokemonAnimation(
      MODE_GAME
    );

    assert(
      desk.pixels ==
      beforePixels +
      wildPixels
    );

    beforePixels =
      desk.pixels;

    hostMillis +=
      150;

    GameApp::updatePokemonAnimation(
      MODE_GAME
    );

    assert(
      desk.pixels ==
      beforePixels +
      wildPixels +
      2 &&
      FakeNvs::writes ==
      feedbackWrites
    );

    // 보고서가 끝나기 전 재부팅해도 저장된 결과/Active로 복원하며,
    // transient UI는 전투 명령 메뉴로 돌아온다.
    const auto status =
      GameApp::state().battle.status;

    GameApp::init();
    GameApp::drawGameTextScreen();

    assert(
      GameApp::state().battle.status ==
      status
    );

    assert(
      game.text.find(
        "데미지!"
      ) ==
        std::string::npos
    );

    if (
      status ==
      BattleStatus::Active
    ) {
      assert(
        gameVisible(
          "행동 선택"
        )
      );
    }
  }

  auto highLevel =
    createNewGame();

  highLevel.party.members[0].level =
    100;

  highLevel.party.members[0].currentHp =
    calculateStats(
      highLevel.party.members[0]
    ).hp;

  assert(
    setEncounter(
      highLevel.encounter,
      19,
      0,
      100,
      Gender::Male,
      false
    )
  );

  assert(
    startBattle(
      highLevel
    )
  );

  assert(
    GameApp::saveState(
      highLevel
    )
  );

  GameApp::init();
  GameApp::drawGameGraphics();
  GameApp::drawGameTextScreen();

  assert(
    desk.text.find(
      "HP 180/180"
    ) !=
    std::string::npos
  );

  enterFight();

  // 턴 save 실패 시 effect/report 없음.
  FakeNvs::partialWrite =
    true;

  GameApp::handleButton(
    BUTTON_OK
  );

  GameApp::drawGameGraphics();

  assert(
    desk.framePixels.size() ==
    5
  );

  assert(
    game.text.find(
      "데미지!"
    ) ==
    std::string::npos
  );

  assert(
    GameApp::state().battle.turn ==
    0
  );

  FakeNvs::partialWrite =
    false;

  // save 실패 뒤에는 MoveSelection에 남아 있으므로 바로 재시도 가능.
  GameApp::handleButton(
    BUTTON_OK
  );

  GameApp::updatePokemonAnimation(
    MODE_GAME
  );

  assert(
    desk.framePixels.size() ==
    2
  );

  for (
    const auto& pixel :
    desk.framePixels
  ) {
    assert(
      pixel.first <
      32
    );
  }

  // 두 번째 action.
  GameApp::handleButton(
    BUTTON_OK
  );

  GameApp::updatePokemonAnimation(
    MODE_GAME
  );

  assert(
    desk.framePixels.size() ==
    3
  );

  for (
    const auto& pixel :
    desk.framePixels
  ) {
    assert(
      pixel.first >=
      102
    );
  }

  // feedback 종료 → 전투 명령 메뉴.
  GameApp::handleButton(
    BUTTON_OK
  );

  GameApp::updatePokemonAnimation(
    MODE_GAME
  );

  assert(
    desk.framePixels.size() ==
    5
  );

  assert(
    gameVisible(
      "행동 선택"
    )
  );

  std::puts(
    "PASS asset app G5-A1: synthetic sprites, bounds, hit effect, DESK animation and battle command return"
  );
}
