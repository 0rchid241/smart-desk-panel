#pragma once
#include "../core/app_types.h"

namespace Buttons {
void init();
// OK: press edge. LEFT/RIGHT: release edge. Chord suppresses both singles.
// Short L+R: BACK on both releases; >=1000ms: MODE_SWITCH, no release BACK.
// Opt-in only for Box browsing; legacy screens keep release-edge holds.
ButtonEvent readButtonEvent(bool allowLong = false);
}
