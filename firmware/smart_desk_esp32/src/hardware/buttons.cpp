#include "buttons.h"
#include "../core/app_config.h"
using namespace AppConfig;
namespace Buttons {
namespace {
const int BUTTON_PINS[3] = {
  BUTTON_LEFT_PIN,
  BUTTON_OK_PIN,
  BUTTON_RIGHT_PIN
};

const ButtonEvent BUTTON_EVENTS[3] = {
  BUTTON_LEFT,
  BUTTON_OK,
  BUTTON_RIGHT
};

bool lastButtonReadings[3] = {
  HIGH,
  HIGH,
  HIGH
};

bool stableButtonStates[3] = {
  HIGH,
  HIGH,
  HIGH
};

unsigned long lastButtonChangeAt[3] = {
  0,
  0,
  0
};




} // namespace

void init() {
  pinMode(
    BUTTON_LEFT_PIN,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_OK_PIN,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_RIGHT_PIN,
    INPUT_PULLUP
  );

}

ButtonEvent readButtonEvent() {

  unsigned long now =
    millis();

  bool pressedEdge[3] = {
    false,
    false,
    false
  };

  bool releasedEdge[3] = {
    false,
    false,
    false
  };


  // -------------------------
  // 모든 버튼 debounce 먼저 처리
  // -------------------------

  for (
    int i = 0;
    i < 3;
    i++
  ) {

    bool reading =
      digitalRead(
        BUTTON_PINS[i]
      );

    if (
      reading !=
      lastButtonReadings[i]
    ) {

      lastButtonChangeAt[i] =
        now;

      lastButtonReadings[i] =
        reading;
    }

    if (
      now -
        lastButtonChangeAt[i] >
        BUTTON_DEBOUNCE_MS
    ) {

      if (
        reading !=
        stableButtonStates[i]
      ) {

        stableButtonStates[i] =
          reading;

        if (
          reading ==
          LOW
        ) {
          pressedEdge[i] =
            true;

        } else {
          releasedEdge[i] =
            true;
        }
      }
    }
  }


  static bool leftPending =
    false;

  static bool rightPending =
    false;

  static bool chordActive =
    false;

  static bool chordTriggered =
    false;

  static unsigned long chordStartedAt =
    0;


  if (
    pressedEdge[0]
  ) {
    leftPending =
      true;
  }

  if (
    pressedEdge[2]
  ) {
    rightPending =
      true;
  }


  bool leftDown =
    stableButtonStates[0] ==
    LOW;

  bool rightDown =
    stableButtonStates[2] ==
    LOW;


  // -------------------------
  // LEFT + RIGHT 길게 누르기
  // -------------------------

  if (
    leftDown &&
    rightDown
  ) {

    if (
      !chordActive
    ) {

      chordActive =
        true;

      chordTriggered =
        false;

      chordStartedAt =
        now;
    }


    if (
      !chordTriggered &&
      now -
        chordStartedAt >=
        MODE_SWITCH_HOLD_MS
    ) {

      chordTriggered =
        true;

      leftPending =
        false;

      rightPending =
        false;

      return
        BUTTON_MODE_SWITCH;
    }


    return
      BUTTON_NONE;
  }


  // 두 버튼을 함께 눌렀다가 뗀 경우에는
  // LEFT/RIGHT 단독 입력으로 처리하지 않는다.
  if (
    chordActive
  ) {

    if (
      !leftDown &&
      !rightDown
    ) {

      chordActive =
        false;

      chordTriggered =
        false;

      leftPending =
        false;

      rightPending =
        false;
    }

    return
      BUTTON_NONE;
  }


  // OK는 기존처럼 누르는 순간 처리한다.
  if (
    pressedEdge[1]
  ) {
    return
      BUTTON_OK;
  }


  // LEFT / RIGHT는 버튼을 뗄 때 단독 입력 확정.
  if (
    releasedEdge[0] &&
    leftPending
  ) {

    leftPending =
      false;

    return
      BUTTON_LEFT;
  }


  if (
    releasedEdge[2] &&
    rightPending
  ) {

    rightPending =
      false;

    return
      BUTTON_RIGHT;
  }


  return
    BUTTON_NONE;
}
} // namespace Buttons
