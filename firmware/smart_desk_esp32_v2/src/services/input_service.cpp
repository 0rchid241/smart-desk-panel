#include "input_service.h"

#include <Arduino.h>

#include "../core/app_config.h"

namespace InputService {
namespace {

constexpr uint8_t BUTTON_COUNT = 3;

const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
    AppConfig::BUTTON_LEFT_PIN,
    AppConfig::BUTTON_OK_PIN,
    AppConfig::BUTTON_RIGHT_PIN,
};

bool rawStates[BUTTON_COUNT] = {
    HIGH,
    HIGH,
    HIGH,
};

bool stableStates[BUTTON_COUNT] = {
    HIGH,
    HIGH,
    HIGH,
};

unsigned long lastRawChangeAt[BUTTON_COUNT] = {
    0,
    0,
    0,
};

bool leftPending = false;
bool rightPending = false;

bool chordActive = false;
bool chordTriggered = false;

unsigned long chordStartedAt = 0;

Event pendingEvent = Event::NONE;

void emit(Event event) {
  if (pendingEvent == Event::NONE) {
    pendingEvent = event;
  }
}

}  // namespace

void init() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);

    rawStates[i] =
        digitalRead(BUTTON_PINS[i]);

    stableStates[i] =
        rawStates[i];

    lastRawChangeAt[i] =
        millis();
  }

  leftPending = false;
  rightPending = false;

  chordActive = false;
  chordTriggered = false;

  pendingEvent = Event::NONE;

  Serial.println("Buttons initialized");
}

void update() {
  const unsigned long now = millis();

  bool pressedEdge[BUTTON_COUNT] = {
      false,
      false,
      false,
  };

  bool releasedEdge[BUTTON_COUNT] = {
      false,
      false,
      false,
  };

  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    const bool reading =
        digitalRead(BUTTON_PINS[i]);

    if (reading != rawStates[i]) {
      rawStates[i] = reading;
      lastRawChangeAt[i] = now;
    }

    if (now - lastRawChangeAt[i] >=
        AppConfig::BUTTON_DEBOUNCE_MS) {
      if (reading != stableStates[i]) {
        stableStates[i] = reading;

        if (reading == LOW) {
          pressedEdge[i] = true;
        } else {
          releasedEdge[i] = true;
        }
      }
    }
  }

  if (pressedEdge[0]) {
    leftPending = true;
  }

  if (pressedEdge[2]) {
    rightPending = true;
  }

  const bool leftDown =
      stableStates[0] == LOW;

  const bool rightDown =
      stableStates[2] == LOW;

  // LEFT + RIGHT chord
  if (leftDown && rightDown) {
    if (!chordActive) {
      chordActive = true;
      chordTriggered = false;
      chordStartedAt = now;
    }

    if (!chordTriggered &&
        now - chordStartedAt >=
            AppConfig::MODE_SWITCH_HOLD_MS) {
      chordTriggered = true;

      leftPending = false;
      rightPending = false;

      emit(Event::MODE_SWITCH);
    }

    return;
  }

  // Chord release.
  if (chordActive) {
    if (!leftDown && !rightDown) {
      const bool shortChord =
          !chordTriggered;

      chordActive = false;
      chordTriggered = false;

      leftPending = false;
      rightPending = false;

      if (shortChord) {
        emit(Event::BACK);
      }
    }

    return;
  }

  // OK uses press edge.
  if (pressedEdge[1]) {
    emit(Event::OK);
    return;
  }

  // LEFT / RIGHT are confirmed on release.
  if (releasedEdge[0] && leftPending) {
    leftPending = false;
    emit(Event::LEFT);
    return;
  }

  if (releasedEdge[2] && rightPending) {
    rightPending = false;
    emit(Event::RIGHT);
    return;
  }
}

Event consumeEvent() {
  const Event event = pendingEvent;
  pendingEvent = Event::NONE;
  return event;
}

}  // namespace InputService
