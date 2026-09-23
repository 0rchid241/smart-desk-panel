#pragma once

#include <time.h>

namespace TimeService {

void init();
void update();

bool isSynced();

time_t epoch();

bool getLocalDateTime(struct tm& result);

}  // namespace TimeService
