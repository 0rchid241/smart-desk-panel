#pragma once

namespace InputService {

enum class Button {
  LEFT,
  OK,
  RIGHT,
};

void init();
void update();

bool consumePressed(Button button);

}  // namespace InputService
