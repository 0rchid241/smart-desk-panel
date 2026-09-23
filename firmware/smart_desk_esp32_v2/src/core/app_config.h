#pragma once

#include <Arduino.h>

namespace AppConfig {

constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 5000;

constexpr char LIN_SERVER_HEALTH_URL[] =
    "http://192.168.35.187:8000/health";

constexpr unsigned long LIN_SERVER_CHECK_INTERVAL_MS = 5000;
constexpr uint16_t LIN_SERVER_HTTP_TIMEOUT_MS = 1000;

constexpr uint8_t BUTTON_LEFT_PIN = 25;
constexpr uint8_t BUTTON_OK_PIN = 26;
constexpr uint8_t BUTTON_RIGHT_PIN = 27;

constexpr unsigned long BUTTON_DEBOUNCE_MS = 30;
constexpr unsigned long BUTTON_LONG_HOLD_MS = 750;
constexpr unsigned long MODE_SWITCH_HOLD_MS = 1000;

constexpr char NTP_SERVER_PRIMARY[] =
    "pool.ntp.org";

constexpr char NTP_SERVER_SECONDARY[] =
    "time.google.com";

constexpr long TIME_GMT_OFFSET_SEC =
    9L * 60L * 60L;

constexpr int TIME_DAYLIGHT_OFFSET_SEC = 0;

}  // namespace AppConfig
