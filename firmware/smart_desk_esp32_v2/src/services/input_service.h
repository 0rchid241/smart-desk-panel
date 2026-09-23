#pragma once

namespace InputService {

enum class Event {
  NONE,
  LEFT,
  OK,
  RIGHT,
  BACK,
  MODE_SWITCH,
};

void init();
void update();

Event consumeEvent();

}  // namespace InputService
