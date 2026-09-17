#pragma once
#include <Arduino.h>
enum ScreenMode {
  SCREEN_HOME,
  SCREEN_CALENDAR,
  SCREEN_TIMER,
  SCREEN_NOTIFICATION
};
enum DeviceMode {
  MODE_DESK,
  MODE_GAME
};
struct ScheduleEvent {
  int year;
  int month;
  int day;

  String title;

  uint8_t reminderFlags;
};

enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_LEFT,
  BUTTON_OK,
  BUTTON_RIGHT,
  BUTTON_MODE_SWITCH,
  BUTTON_LEFT_LONG,
  BUTTON_RIGHT_LONG,
  BUTTON_BACK
};

using MessageHandler = void (*)(const char*, const char*);
using NotificationHandler = void (*)(const String&, const String&);
