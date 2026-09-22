#include "network_service.h"

#include <Arduino.h>
#include <WiFi.h>

#include "../core/app_config.h"
#include "../../wifi_secrets.h"

namespace NetworkService {
namespace {

bool wifiWasConnected = false;
unsigned long lastReconnectAttemptAt = 0;

void startWifiConnection() {
  Serial.println("Wi-Fi connection start");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lastReconnectAttemptAt = millis();
}

}  // namespace

void init() {
  startWifiConnection();
}

void update() {
  const bool wifiConnected =
      WiFi.status() == WL_CONNECTED;

  if (wifiConnected && !wifiWasConnected) {
    Serial.println("Wi-Fi connected");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  }

  if (!wifiConnected && wifiWasConnected) {
    Serial.println("Wi-Fi disconnected");
  }

  if (!wifiConnected &&
      millis() - lastReconnectAttemptAt >=
          AppConfig::WIFI_RECONNECT_INTERVAL_MS) {
    Serial.println("Wi-Fi reconnect attempt");

    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    lastReconnectAttemptAt = millis();
  }

  wifiWasConnected = wifiConnected;
}

bool isWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

}  // namespace NetworkService
