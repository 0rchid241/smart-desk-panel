#include "desk_app.h"
#include "../hardware/displays.h"
#include "../services/calendar_service.h"
#include "../services/network_time.h"
#include "../core/app_config.h"
#include "../../hangul_renderer.h"
using namespace AppConfig;
using namespace CalendarService;
using NetworkTime::getCurrentTimeInfo;
namespace DeskApp {
namespace {
ScreenMode currentScreen =
  SCREEN_HOME;
int calendarPage =
  0;
String notificationTitle =
  "";

String notificationMessage =
  "";

ScreenMode screenBeforeNotification =
  SCREEN_HOME;
bool timerRunning =
  false;

unsigned long timerStartedAt =
  0;

void showHome();
void showCalendar();
void startTimer();
unsigned long getTimerRemainingSeconds();
void showTimer();
void showNotification();
void moveMainScreen(
  ButtonEvent button
);

void showHome() {

  struct tm timeinfo;

  bool timeAvailable =
    getCurrentTimeInfo(
      timeinfo
    );

  char dateText[20] =
    "--/-- ---";

  char timeText[10] =
    "--:--";

  if (timeAvailable) {
    strftime(
      dateText,
      sizeof(dateText),
      "%m/%d %a",
      &timeinfo
    );

    strftime(
      timeText,
      sizeof(timeText),
      "%H:%M",
      &timeinfo
    );
  }

  Displays::desk().clearDisplay();

  Displays::desk().setTextColor(
    SSD1306_WHITE
  );

  Displays::desk().setTextSize(1);

  Displays::desk().setCursor(
    0,
    0
  );

  Displays::desk().print(
    dateText
  );

  Displays::desk().setCursor(
    98,
    0
  );

  if (
    NetworkTime::isConnected()
  ) {
    Displays::desk().print(
      "WiFi"
    );
  } else {
    Displays::desk().print(
      "OFF"
    );
  }

  Displays::desk().setTextSize(2);

  Displays::desk().setCursor(
    34,
    13
  );

  Displays::desk().print(
    timeText
  );

  Displays::desk().drawLine(
    0,
    32,
    127,
    32,
    SSD1306_WHITE
  );

  int nextIndex =
    findNextEventIndex();

  Displays::desk().setTextSize(1);

  if (
    nextIndex >= 0
  ) {

    const ScheduleEvent& event = CalendarService::getEvent(nextIndex);

    Displays::desk().setCursor(
      0,
      35
    );

    Displays::desk().print(
      "NEXT "
    );

    if (timeAvailable) {
      Displays::desk().print(
        getDDayText(
          getDaysUntil(
            event
          )
        )
      );

    } else {
      Displays::desk().print(
        getEventDateText(
          event
        )
      );
    }

    drawScrollingUtf8Text(
      Displays::desk(),
      0,
      46,
      128,
      event.title
    );

  } else {

    Displays::desk().setCursor(
      0,
      36
    );

    Displays::desk().print(
      "NEXT"
    );

    drawUtf8Text(
      Displays::desk(),
      0,
      46,
      "예정 없음"
    );
  }

  Displays::desk().display();
}

void showCalendar() {

  struct tm timeinfo;

  bool timeAvailable =
    getCurrentTimeInfo(
      timeinfo
    );

  char dateText[10] =
    "--/--";

  if (timeAvailable) {
    strftime(
      dateText,
      sizeof(dateText),
      "%m/%d",
      &timeinfo
    );
  }

  int pageCount =
    getCalendarPageCount();

  if (
    calendarPage >=
    pageCount
  ) {
    calendarPage = 0;
  }

  Displays::desk().clearDisplay();

  Displays::desk().setTextColor(
    SSD1306_WHITE
  );

  drawUtf8Text(
    Displays::desk(),
    0,
    0,
    "일정"
  );

  Displays::desk().setTextSize(1);

  Displays::desk().setCursor(
    84,
    4
  );

  Displays::desk().print(
    dateText
  );

  Displays::desk().drawLine(
    0,
    18,
    127,
    18,
    SSD1306_WHITE
  );

  int firstOrder =
    calendarPage *
    CALENDAR_EVENTS_PER_PAGE;

  bool eventShown =
    false;

  for (
    int row = 0;
    row <
      CALENDAR_EVENTS_PER_PAGE;
    row++
  ) {

    int order =
      firstOrder +
      row;

    int eventIndex =
      findUpcomingEventIndexByOrder(
        order
      );

    if (
      eventIndex < 0
    ) {
      continue;
    }

    eventShown =
      true;

    const ScheduleEvent& event = CalendarService::getEvent(eventIndex);

    int y =
      21 +
      row * 18;

    Displays::desk().setTextSize(1);

    Displays::desk().setCursor(
      0,
      y + 4
    );

    if (timeAvailable) {
      Displays::desk().print(
        getDDayText(
          getDaysUntil(
            event
          )
        )
      );

    } else {
      Displays::desk().print(
        getEventDateText(
          event
        )
      );
    }

    drawScrollingUtf8Text(
      Displays::desk(),
      34,
      y,
      94,
      event.title
    );
  }

  if (
    !eventShown
  ) {
    drawUtf8Text(
      Displays::desk(),
      0,
      25,
      "예정 없음"
    );
  }

  Displays::desk().setTextSize(1);

  Displays::desk().setCursor(
    0,
    56
  );

  Displays::desk().print("<");

  char pageText[12];

  snprintf(
    pageText,
    sizeof(pageText),
    "%d/%d OK",
    calendarPage + 1,
    pageCount
  );

  Displays::desk().setCursor(
    45,
    56
  );

  Displays::desk().print(
    pageText
  );

  Displays::desk().setCursor(
    122,
    56
  );

  Displays::desk().print(">");

  Displays::desk().display();
}

void startTimer() {

  timerStartedAt =
    millis();

  timerRunning =
    true;

  Serial.println(
    "Timer started"
  );
}

unsigned long getTimerRemainingSeconds() {

  if (
    !timerRunning
  ) {
    return
      TIMER_DURATION_MS /
      1000;
  }


  unsigned long elapsed =
    millis() -
    timerStartedAt;


  if (
    elapsed >=
    TIMER_DURATION_MS
  ) {
    return 0;
  }


  unsigned long remainingMs =
    TIMER_DURATION_MS -
    elapsed;


  return
    (remainingMs + 999) /
    1000;
}

void showTimer() {

  unsigned long remainingSeconds =
    getTimerRemainingSeconds();


  unsigned int minutes =
    remainingSeconds /
    60;

  unsigned int seconds =
    remainingSeconds %
    60;


  char timerText[10];

  snprintf(
    timerText,
    sizeof(timerText),
    "%02u:%02u",
    minutes,
    seconds
  );


  Displays::desk().clearDisplay();

  Displays::desk().setTextColor(
    SSD1306_WHITE
  );

  Displays::desk().setTextSize(1);

  Displays::desk().setCursor(
    0,
    0
  );

  Displays::desk().println(
    "TIMER"
  );


  Displays::desk().drawLine(
    0,
    11,
    127,
    11,
    SSD1306_WHITE
  );


  Displays::desk().setTextSize(2);

  Displays::desk().setCursor(
    34,
    22
  );

  Displays::desk().println(
    timerText
  );


  Displays::desk().setTextSize(1);


  if (
    timerRunning
  ) {

    Displays::desk().setCursor(
      38,
      44
    );

    Displays::desk().println(
      "RUNNING"
    );

  } else {

    Displays::desk().setCursor(
      34,
      44
    );

    Displays::desk().println(
      "OK START"
    );
  }


  Displays::desk().setCursor(
    0,
    56
  );

  Displays::desk().print(
    "< CAL"
  );


  Displays::desk().setCursor(
    86,
    56
  );

  Displays::desk().print(
    "HOME >"
  );


  Displays::desk().display();
}

void showNotification() {

  Displays::desk().clearDisplay();

  Displays::desk().setTextColor(
    SSD1306_WHITE
  );


  // 알림 제목
  drawUtf8Text(
    Displays::desk(),
    0,
    0,
    notificationTitle
  );


  Displays::desk().drawLine(
    0,
    18,
    127,
    18,
    SSD1306_WHITE
  );


  // 알림 내용
  drawScrollingUtf8Text(
      Displays::desk(),
      0,
      24,
      128,
      notificationMessage
    );


  // 하단 안내
  Displays::desk().setTextSize(1);

  Displays::desk().setCursor(
    30,
    56
  );

  Displays::desk().print(
    "OK DISMISS"
  );


  Displays::desk().display();
}

void moveMainScreen(
  ButtonEvent button
) {

  if (
    button ==
    BUTTON_RIGHT
  ) {

    if (
      currentScreen ==
      SCREEN_HOME
    ) {

      currentScreen =
        SCREEN_CALENDAR;

    }

    else if (
      currentScreen ==
      SCREEN_CALENDAR
    ) {

      currentScreen =
        SCREEN_TIMER;

    }

    else if (
      currentScreen ==
      SCREEN_TIMER
    ) {

      currentScreen =
        SCREEN_HOME;
    }
  }


  else if (
    button ==
    BUTTON_LEFT
  ) {

    if (
      currentScreen ==
      SCREEN_HOME
    ) {

      currentScreen =
        SCREEN_TIMER;

    }

    else if (
      currentScreen ==
      SCREEN_TIMER
    ) {

      currentScreen =
        SCREEN_CALENDAR;

    }

    else if (
      currentScreen ==
      SCREEN_CALENDAR
    ) {

      currentScreen =
        SCREEN_HOME;
    }
  }
}
} // namespace

void init() { currentScreen = SCREEN_HOME; }

void showMessage(
  const char* line1,
  const char* line2
) {
  Displays::desk().clearDisplay();

  Displays::desk().setTextSize(1);

  Displays::desk().setTextColor(
    SSD1306_WHITE
  );

  Displays::desk().setCursor(
    0,
    0
  );

  Displays::desk().println(
    "SMART DESK"
  );

  Displays::desk().setCursor(
    0,
    20
  );

  Displays::desk().println(
    line1
  );

  Displays::desk().setCursor(
    0,
    35
  );

  Displays::desk().println(
    line2
  );

  Displays::desk().display();
}

void triggerNotification(
  const String& title,
  const String& message
) {

  if (
    currentScreen !=
    SCREEN_NOTIFICATION
  ) {
    screenBeforeNotification =
      currentScreen;
  }


  notificationTitle =
    title;

  notificationMessage =
    message;

  currentScreen =
    SCREEN_NOTIFICATION;


  Serial.print(
    "Notification: "
  );

  Serial.print(
    title
  );

  Serial.print(
    " / "
  );

  Serial.println(
    message
  );
}

void showGameNotification() {

  Displays::game().clearDisplay();

  Displays::game().setTextColor(
    SSD1306_WHITE
  );

  drawUtf8Text(
    Displays::game(),
    0,
    0,
    notificationTitle
  );

  Displays::game().drawLine(
    0,
    18,
    127,
    18,
    SSD1306_WHITE
  );

  drawScrollingUtf8Text(
    Displays::game(),
    0,
    24,
    128,
    notificationMessage
  );

  Displays::game().setTextSize(1);

  Displays::game().setCursor(
    30,
    56
  );

  Displays::game().print(
    "OK DISMISS"
  );

  Displays::game().display();
}

void update() {
  // 타이머는 다른 화면에서도 계속 진행
  if (
    timerRunning &&
    millis() -
      timerStartedAt >=
      TIMER_DURATION_MS
  ) {

    timerRunning =
      false;

    triggerNotification(
      "TIMER",
      "Timer finished"
    );
  }


}

void render() {
    // OLED 1 = Smart Desk
    if (
      currentScreen ==
      SCREEN_HOME
    ) {
      showHome();

    } else if (
      currentScreen ==
      SCREEN_CALENDAR
    ) {
      showCalendar();

    } else if (
      currentScreen ==
      SCREEN_TIMER
    ) {
      showTimer();

    } else if (
      currentScreen ==
      SCREEN_NOTIFICATION
    ) {
      showNotification();
    }

}

void handleButton(ButtonEvent button) {
  // CALENDAR에서 OK → 다음 페이지
  if (
    currentScreen ==
      SCREEN_CALENDAR &&
    button ==
      BUTTON_OK
  ) {

    int pageCount =
      getCalendarPageCount();

    if (
      pageCount > 1
    ) {

      calendarPage =
        (
          calendarPage + 1
        ) %
        pageCount;
    }

    return;
  }


  // LEFT / RIGHT → Smart Desk 화면 이동
  if (
    button ==
      BUTTON_LEFT ||
    button ==
      BUTTON_RIGHT
  ) {

    moveMainScreen(
      button
    );

    return;
  }


  // TIMER에서 OK → 시작
  if (
    currentScreen ==
      SCREEN_TIMER &&
    button ==
      BUTTON_OK &&
    !timerRunning
  ) {

    startTimer();

    return;
  }
}

bool notificationActive() { return currentScreen == SCREEN_NOTIFICATION; }

void dismissNotification() {
  currentScreen = screenBeforeNotification;
  Serial.println("Notification dismissed");
}
} // namespace DeskApp
