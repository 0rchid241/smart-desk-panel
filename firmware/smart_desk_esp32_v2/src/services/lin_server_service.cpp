#include "lin_server_service.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include "../core/app_config.h"
#include "network_service.h"

namespace LinServerService {
namespace {

constexpr uint8_t OFFLINE_FAILURE_THRESHOLD = 3;

volatile bool serverAvailable = false;

bool lastReportedAvailable = false;
bool hasReportedState = false;

uint8_t consecutiveFailures = 0;

void reportState(bool available) {
  if (!hasReportedState ||
      available != lastReportedAvailable) {
    Serial.print("Lin server: ");
    Serial.println(available ? "ONLINE" : "OFFLINE");

    lastReportedAvailable = available;
    hasReportedState = true;
  }
}

bool checkHealth() {
  if (!NetworkService::isWifiConnected()) {
    return false;
  }

  HTTPClient http;

  http.setConnectTimeout(
      AppConfig::LIN_SERVER_HTTP_TIMEOUT_MS);
  http.setTimeout(
      AppConfig::LIN_SERVER_HTTP_TIMEOUT_MS);

  if (!http.begin(AppConfig::LIN_SERVER_HEALTH_URL)) {
    return false;
  }

  const int statusCode = http.GET();
  http.end();

  return statusCode == 200;
}

void updateServerState(bool healthOk) {
  if (healthOk) {
    consecutiveFailures = 0;

    if (!serverAvailable) {
      serverAvailable = true;
      reportState(true);
    }

    return;
  }

  if (consecutiveFailures < OFFLINE_FAILURE_THRESHOLD) {
    ++consecutiveFailures;
  }

  if (serverAvailable &&
      consecutiveFailures >= OFFLINE_FAILURE_THRESHOLD) {
    serverAvailable = false;
    reportState(false);
  }
}

void serverTask(void* parameter) {
  (void)parameter;

  for (;;) {
    if (!NetworkService::isWifiConnected()) {
      consecutiveFailures = OFFLINE_FAILURE_THRESHOLD;

      if (serverAvailable) {
        serverAvailable = false;
        reportState(false);
      }
    } else {
      updateServerState(checkHealth());
    }

    vTaskDelay(
        pdMS_TO_TICKS(
            AppConfig::LIN_SERVER_CHECK_INTERVAL_MS));
  }
}

}  // namespace

void init() {
  reportState(false);

  xTaskCreate(
      serverTask,
      "lin-server",
      6144,
      nullptr,
      1,
      nullptr);
}

void update() {
  // HTTP polling runs in its own task so the main loop stays responsive.
}

bool isAvailable() {
  return serverAvailable;
}

}  // namespace LinServerService
