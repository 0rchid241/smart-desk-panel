#include "network_time.h"
#include "../core/app_config.h"
#include "../../wifi_secrets.h"
#include <WiFi.h>
using namespace AppConfig;
namespace NetworkTime {
namespace {
unsigned long lastWifiReconnectAt =
  0;

bool wifiWasConnected =
  false;

MessageHandler showMessage = nullptr;
bool connectWiFi();
void syncTime();

bool connectWiFi() {

  showMessage(
    "Wi-Fi",
    "Connecting..."
  );

  WiFi.mode(
    WIFI_STA
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long startedAt =
    millis();

  while (
    WiFi.status() !=
      WL_CONNECTED &&
    millis() - startedAt <
      WIFI_CONNECT_TIMEOUT_MS
  ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    Serial.println(
      "Wi-Fi connect timeout"
    );

    showMessage(
      "Wi-Fi",
      "OFFLINE MODE"
    );

    delay(1000);

    return false;
  }

  Serial.println(
    "Wi-Fi connected"
  );

  Serial.println(
    WiFi.localIP()
  );

  showMessage(
    "Wi-Fi",
    "CONNECTED"
  );

  delay(1000);

  return true;
}

void syncTime() {

  showMessage(
    "TIME",
    "Syncing..."
  );

  configTime(
    9 * 3600,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  struct tm timeinfo;

  if (
    !getLocalTime(
      &timeinfo,
      10000
    )
  ) {

    Serial.println(
      "Time sync failed"
    );

    showMessage(
      "TIME",
      "SYNC FAILED"
    );

    return;
  }

  Serial.println(
    "Time sync success"
  );
}
} // namespace

bool init(MessageHandler messageHandler) {
  showMessage = messageHandler;
  bool wifiConnected =
    false;

  if (
    FORCE_OFFLINE_TEST_MODE
  ) {
    Serial.println(
      "FORCE OFFLINE TEST MODE"
    );

  } else {
    wifiConnected =
      connectWiFi();
  }

  wifiWasConnected =
    wifiConnected;

  if (wifiConnected) {
    syncTime();
  }


  return wifiConnected;
}

void startReconnectClock() { lastWifiReconnectAt = millis(); }

bool update() {
  bool reconnected = false;
  bool wifiConnected =
    WiFi.status() ==
    WL_CONNECTED;

  if (
    !FORCE_OFFLINE_TEST_MODE &&
    !wifiConnected &&
    millis() -
      lastWifiReconnectAt >=
      WIFI_RECONNECT_INTERVAL_MS
  ) {
    lastWifiReconnectAt =
      millis();

    Serial.println(
      "Wi-Fi reconnect attempt"
    );

    WiFi.disconnect();

    WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
    );
  }


  // 오프라인 상태에서 Wi-Fi가 돌아온 순간
  if (
    wifiConnected &&
    !wifiWasConnected
  ) {
    Serial.println(
      "Wi-Fi reconnected"
    );

    syncTime();

    reconnected = true;


  }

  wifiWasConnected =
    wifiConnected;



  return reconnected;
}

bool isConnected() { return WiFi.status() == WL_CONNECTED; }

bool getCurrentTimeInfo(
  struct tm& timeinfo
) {
  time_t now =
    time(nullptr);

  // NTP 동기화 전 ESP32는 1970년대 값에 가깝다.
  if (
    now < 1700000000
  ) {
    return false;
  }

  localtime_r(
    &now,
    &timeinfo
  );

  return true;
}
} // namespace NetworkTime
