#pragma once
#include <Arduino.h>
namespace AppConfig {
constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr int OLED_RESET = -1;
constexpr int SCREEN_ADDRESS = 0x3C;

constexpr int BUTTON_LEFT_PIN = 25;
constexpr int BUTTON_OK_PIN = 26;
constexpr int BUTTON_RIGHT_PIN = 27;

const bool RESET_REMINDER_STORAGE_ON_BOOT =
  false;

// 오프라인 캐시 테스트용. 최종값은 false 유지.
const bool FORCE_OFFLINE_TEST_MODE =
  false;

const int MAX_SCHEDULE_EVENTS = 20;
const int CALENDAR_EVENTS_PER_PAGE =
  2;
const uint8_t REMINDER_D3 =
  1 << 0;

const uint8_t REMINDER_D1 =
  1 << 1;

const uint8_t REMINDER_DDAY =
  1 << 2;
const unsigned long BUTTON_DEBOUNCE_MS =
  30;

// LEFT + RIGHT를 이 시간 이상 동시에 누르면
// DESK ↔ GAME 모드를 전환한다.
const unsigned long MODE_SWITCH_HOLD_MS =
  1000;
// 아직 테스트용 10초
const unsigned long TIMER_DURATION_MS =
  10000;
// 최종 자동 동기화 주기: 15분
const unsigned long CALENDAR_SYNC_INTERVAL_MS =
  15UL * 60UL * 1000UL;
const unsigned long WIFI_CONNECT_TIMEOUT_MS =
  12000;

const unsigned long WIFI_RECONNECT_INTERVAL_MS =
  30000;

constexpr int DESK_SDA_PIN = 21;
constexpr int DESK_SCL_PIN = 22;
constexpr int GAME_SDA_PIN = 16;
constexpr int GAME_SCL_PIN = 17;
}
