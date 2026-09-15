#pragma once
#include "../core/app_types.h"

namespace Buttons {
void init();
// OK: press edge. LEFT/RIGHT: release edge. Chord suppresses both singles.
ButtonEvent readButtonEvent();
}
