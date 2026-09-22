#pragma once

#include <Arduino.h>

namespace AppConfig {

constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 5000;

constexpr char LIN_SERVER_HEALTH_URL[] =
    "http://192.168.35.187:8000/health";

constexpr unsigned long LIN_SERVER_CHECK_INTERVAL_MS = 5000;
constexpr uint16_t LIN_SERVER_HTTP_TIMEOUT_MS = 1000;

}  // namespace AppConfig
