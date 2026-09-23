#include "time_service.h"

#include <Arduino.h>
#include <time.h>

#include "../core/app_config.h"
#include "network_service.h"

namespace TimeService {
namespace {

constexpr time_t MIN_VALID_EPOCH = 1704067200;
// 2024-01-01 00:00:00 UTC

bool syncStarted = false;
bool synced = false;

void startSync() {
  configTime(
      AppConfig::TIME_GMT_OFFSET_SEC,
      AppConfig::TIME_DAYLIGHT_OFFSET_SEC,
      AppConfig::NTP_SERVER_PRIMARY,
      AppConfig::NTP_SERVER_SECONDARY);

  syncStarted = true;

  Serial.println("NTP sync started");
}

void printCurrentTime(time_t current) {
  struct tm localTime;

  if (localtime_r(&current, &localTime) == nullptr) {
    return;
  }

  char buffer[32];

  strftime(
      buffer,
      sizeof(buffer),
      "%Y-%m-%d %H:%M:%S",
      &localTime);

  Serial.print("Time synchronized: ");
  Serial.println(buffer);
}

}  // namespace

void init() {
  syncStarted = false;
  synced = false;

  Serial.println("Time service initialized");
}

void update() {
  if (!NetworkService::isWifiConnected()) {
    return;
  }

  if (!syncStarted) {
    startSync();
  }

  if (synced) {
    return;
  }

  const time_t current = time(nullptr);

  if (current < MIN_VALID_EPOCH) {
    return;
  }

  synced = true;

  printCurrentTime(current);
}

bool isSynced() {
  return synced;
}

time_t epoch() {
  if (!synced) {
    return 0;
  }

  return time(nullptr);
}

bool getLocalDateTime(struct tm& result) {
  if (!synced) {
    return false;
  }

  const time_t current = time(nullptr);

  return localtime_r(&current, &result) != nullptr;
}

}  // namespace TimeService
