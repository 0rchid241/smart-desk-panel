#pragma once
#include "../core/app_types.h"
#include <time.h>

namespace NetworkTime {
// Boot messages are supplied by the entry point; this service owns no UI.
bool init(MessageHandler showMessage);
void startReconnectClock();
// Returns true only on reconnection, after the original NTP sync.
bool update();
bool isConnected();
bool getCurrentTimeInfo(struct tm& timeinfo);
}
