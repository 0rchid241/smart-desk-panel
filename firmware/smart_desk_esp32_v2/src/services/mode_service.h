#pragma once

namespace ModeService {

enum class Mode {
  SMART_DESK,
  DESKMON,
};

void init();

void toggle();

Mode current();

const char* currentName();

}  // namespace ModeService
