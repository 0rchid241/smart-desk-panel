#include "input_service.h"

#include <Arduino.h>

#include "../core/app_config.h"

namespace InputService {
namespace {

struct ButtonState {
  uint8_t pin;
  bool rawPressed;
  bool stablePressed;
  bool pressedEvent;
  unsigned long lastRawChangeAt;
};

ButtonState leftButton{
    AppConfig::BUTTON_LEFT_PIN,
    false,
    false,
    false,
    0};

ButtonState okButton{
    AppConfig::BUTTON_OK_PIN,
    false,
    false,
    false,
    0};

ButtonState rightButton{
    AppConfig::BUTTON_RIGHT_PIN,
    false,
    false,
    false,
    0};

ButtonState& getButton(Button button) {
  switch (button) {
    case Button::LEFT:
      return leftButton;

    case Button::OK:
      return okButton;

    case Button::RIGHT:
      return rightButton;
  }

  return okButton;
}

void initializeButton(ButtonState& button) {
  pinMode(button.pin, INPUT_PULLUP);

  const bool pressed =
      digitalRead(button.pin) == LOW;

  button.rawPressed = pressed;
  button.stablePressed = pressed;
  button.pressedEvent = false;
  button.lastRawChangeAt = millis();
}

void updateButton(ButtonState& button) {
  const bool rawPressed =
      digitalRead(button.pin) == LOW;

  if (rawPressed != button.rawPressed) {
    button.rawPressed = rawPressed;
    button.lastRawChangeAt = millis();
  }

  if (button.rawPressed != button.stablePressed &&
      millis() - button.lastRawChangeAt >=
          AppConfig::BUTTON_DEBOUNCE_MS) {
    button.stablePressed = button.rawPressed;

    if (button.stablePressed) {
      button.pressedEvent = true;
    }
  }
}

}  // namespace

void init() {
  initializeButton(leftButton);
  initializeButton(okButton);
  initializeButton(rightButton);

  Serial.println("Buttons initialized");
}

void update() {
  updateButton(leftButton);
  updateButton(okButton);
  updateButton(rightButton);
}

bool consumePressed(Button button) {
  ButtonState& state = getButton(button);

  if (!state.pressedEvent) {
    return false;
  }

  state.pressedEvent = false;
  return true;
}

}  // namespace InputService
