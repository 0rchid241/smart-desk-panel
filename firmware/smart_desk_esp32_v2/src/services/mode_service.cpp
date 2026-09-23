#include "mode_service.h"

#include <Arduino.h>

namespace ModeService {
namespace {

Mode currentMode =
    Mode::SMART_DESK;

}  // namespace

void init() {
  currentMode =
      Mode::SMART_DESK;

  Serial.print("Mode initialized: ");
  Serial.println(currentName());
}

void toggle() {
  if (currentMode ==
      Mode::SMART_DESK) {
    currentMode =
        Mode::DESKMON;
  } else {
    currentMode =
        Mode::SMART_DESK;
  }

  Serial.print("Mode changed: ");
  Serial.println(currentName());
}

Mode current() {
  return currentMode;
}

const char* currentName() {
  switch (currentMode) {
    case Mode::SMART_DESK:
      return "SMART DESK";

    case Mode::DESKMON:
      return "DESKMON";
  }

  return "UNKNOWN";
}

}  // namespace ModeService
