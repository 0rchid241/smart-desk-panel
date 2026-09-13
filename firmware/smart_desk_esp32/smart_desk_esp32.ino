#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

#include "wifi_secrets.h"
#include "hangul_renderer.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUTTON_LEFT_PIN 25
#define BUTTON_OK_PIN 26
#define BUTTON_RIGHT_PIN 27

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);


// --------------------------------------------------
// 화면 상태
// --------------------------------------------------

enum ScreenMode {
  SCREEN_HOME,
  SCREEN_CALENDAR,
  SCREEN_TIMER,
  SCREEN_NOTIFICATION
};

ScreenMode currentScreen = SCREEN_HOME;


// --------------------------------------------------
// 날짜 기반 일정 데이터
// --------------------------------------------------

struct ScheduleEvent {
  int year;
  int month;
  int day;
  String title;

  uint8_t reminderFlags;
};


// 현재는 테스트 데이터.
// 이후 Google Calendar에서 받아온 데이터로 교체한다.
ScheduleEvent scheduleEvents[] = {
  {
    2026,
    9,
    15,
    "머신러닝 과제",
    0
  },
  {
    2026,
    9,
    18,
    "캡스톤 발표",
    0
  },
  {
    2026,
    9,
    25,
    "졸업작품 점검",
    0
  }
};

const int SCHEDULE_EVENT_COUNT =
  sizeof(scheduleEvents) /
  sizeof(scheduleEvents[0]);

const int CALENDAR_EVENTS_PER_PAGE = 2;

int calendarPage = 0;

const uint8_t REMINDER_D3 =
  1 << 0;

const uint8_t REMINDER_D1 =
  1 << 1;

const uint8_t REMINDER_DDAY =
  1 << 2;


// --------------------------------------------------
// 알림 상태
// --------------------------------------------------

String notificationTitle = "";
String notificationMessage = "";

ScreenMode screenBeforeNotification =
  SCREEN_HOME;


// --------------------------------------------------
// 버튼 이벤트
// --------------------------------------------------

enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_LEFT,
  BUTTON_OK,
  BUTTON_RIGHT
};

const int BUTTON_PINS[3] = {
  BUTTON_LEFT_PIN,
  BUTTON_OK_PIN,
  BUTTON_RIGHT_PIN
};

const ButtonEvent BUTTON_EVENTS[3] = {
  BUTTON_LEFT,
  BUTTON_OK,
  BUTTON_RIGHT
};

bool lastButtonReadings[3] = {
  HIGH,
  HIGH,
  HIGH
};

bool stableButtonStates[3] = {
  HIGH,
  HIGH,
  HIGH
};

unsigned long lastButtonChangeAt[3] = {
  0,
  0,
  0
};

const unsigned long BUTTON_DEBOUNCE_MS =
  30;


// --------------------------------------------------
// 타이머 상태
// --------------------------------------------------

bool timerRunning = false;

unsigned long timerStartedAt = 0;

// 아직 테스트용 10초
const unsigned long TIMER_DURATION_MS =
  10000;


// --------------------------------------------------
// 일반 메시지
// --------------------------------------------------

void showMessage(
  const char* line1,
  const char* line2 = ""
) {
  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(
    0,
    0
  );

  display.println(
    "SMART DESK"
  );

  display.setCursor(
    0,
    20
  );

  display.println(
    line1
  );

  display.setCursor(
    0,
    35
  );

  display.println(
    line2
  );

  display.display();
}


// --------------------------------------------------
// Wi-Fi
// --------------------------------------------------

void connectWiFi() {
  showMessage(
    "Wi-Fi",
    "Connecting..."
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  while (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

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
}


// --------------------------------------------------
// NTP 시간
// --------------------------------------------------

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


// --------------------------------------------------
// 날짜 계산
// --------------------------------------------------

bool isLeapYear(
  int year
) {
  return
    (
      year % 4 == 0 &&
      year % 100 != 0
    ) ||
    (
      year % 400 == 0
    );
}


// 날짜를 일련번호 형태로 바꾼다.
// 시간/분/초와 관계없이 날짜 차이만 계산하기 위함.
long dateToDayNumber(
  int year,
  int month,
  int day
) {
  static const int DAYS_BEFORE_MONTH[] = {
    0,
    0,
    31,
    59,
    90,
    120,
    151,
    181,
    212,
    243,
    273,
    304,
    334
  };

  long previousYear =
    year - 1;

  long days =
    previousYear * 365L +
    previousYear / 4 -
    previousYear / 100 +
    previousYear / 400;

  days +=
    DAYS_BEFORE_MONTH[month];

  if (
    month > 2 &&
    isLeapYear(year)
  ) {
    days += 1;
  }

  days += day;

  return days;
}


// --------------------------------------------------
// 오늘로부터 일정까지 며칠 남았는지 계산
// --------------------------------------------------

int getDaysUntil(
  const ScheduleEvent& event
) {
  struct tm timeinfo;

  if (
    !getLocalTime(
      &timeinfo
    )
  ) {
    return 99999;
  }

  int currentYear =
    timeinfo.tm_year + 1900;

  int currentMonth =
    timeinfo.tm_mon + 1;

  int currentDay =
    timeinfo.tm_mday;

  long today =
    dateToDayNumber(
      currentYear,
      currentMonth,
      currentDay
    );

  long eventDay =
    dateToDayNumber(
      event.year,
      event.month,
      event.day
    );

  return
    static_cast<int>(
      eventDay - today
    );
}


// --------------------------------------------------
// 앞으로 남은 일정 개수
// --------------------------------------------------

int countUpcomingEvents() {
  int count = 0;

  for (
    int i = 0;
    i < SCHEDULE_EVENT_COUNT;
    i++
  ) {
    int daysUntil =
      getDaysUntil(
        scheduleEvents[i]
      );

    if (
      daysUntil >= 0
    ) {
      count++;
    }
  }

  return count;
}


// --------------------------------------------------
// n번째로 가까운 일정 찾기
// --------------------------------------------------

int findUpcomingEventIndexByOrder(
  int targetOrder
) {
  for (
    int candidateIndex = 0;
    candidateIndex <
      SCHEDULE_EVENT_COUNT;
    candidateIndex++
  ) {
    int candidateDays =
      getDaysUntil(
        scheduleEvents[
          candidateIndex
        ]
      );

    if (
      candidateDays < 0
    ) {
      continue;
    }

    int rank = 0;

    for (
      int otherIndex = 0;
      otherIndex <
        SCHEDULE_EVENT_COUNT;
      otherIndex++
    ) {
      if (
        otherIndex ==
        candidateIndex
      ) {
        continue;
      }

      int otherDays =
        getDaysUntil(
          scheduleEvents[
            otherIndex
          ]
        );

      if (
        otherDays < 0
      ) {
        continue;
      }

      // 더 가까운 일정이면
      // candidate보다 앞에 위치
      if (
        otherDays <
          candidateDays
      ) {
        rank++;
      }

      // 같은 날짜 일정은
      // 배열 순서를 유지
      else if (
        otherDays ==
          candidateDays &&
        otherIndex <
          candidateIndex
      ) {
        rank++;
      }
    }

    if (
      rank ==
      targetOrder
    ) {
      return
        candidateIndex;
    }
  }

  return -1;
}


// --------------------------------------------------
// 가장 가까운 일정
// --------------------------------------------------

int findNextEventIndex() {
  return
    findUpcomingEventIndexByOrder(
      0
    );
}


// --------------------------------------------------
// CALENDAR 페이지 개수
// --------------------------------------------------

int getCalendarPageCount() {
  int eventCount =
    countUpcomingEvents();

  if (
    eventCount == 0
  ) {
    return 1;
  }

  return
    (
      eventCount +
      CALENDAR_EVENTS_PER_PAGE -
      1
    ) /
    CALENDAR_EVENTS_PER_PAGE;
}


// --------------------------------------------------
// D-Day 문자열 만들기
// --------------------------------------------------

String getDDayText(
  int daysUntil
) {
  if (
    daysUntil == 0
  ) {
    return "D-DAY";
  }

  return
    "D-" +
    String(daysUntil);
}


uint8_t getReminderFlag(
  int daysUntil
) {
  if (
    daysUntil == 3
  ) {
    return REMINDER_D3;
  }

  if (
    daysUntil == 1
  ) {
    return REMINDER_D1;
  }

  if (
    daysUntil == 0
  ) {
    return REMINDER_DDAY;
  }

  return 0;
}


// --------------------------------------------------
// 일정 날짜 표시용 문자열
// --------------------------------------------------

String getEventDateText(
  const ScheduleEvent& event
) {
  char buffer[6];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02d/%02d",
    event.month,
    event.day
  );

  return String(buffer);
}


// --------------------------------------------------
// HOME
// --------------------------------------------------

void showHome() {
  struct tm timeinfo;

  if (
    !getLocalTime(
      &timeinfo
    )
  ) {
    showMessage(
      "TIME",
      "NOT AVAILABLE"
    );

    return;
  }

  char dateText[20];
  char timeText[10];

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

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  // 날짜
  display.setCursor(
    0,
    0
  );

  display.print(
    dateText
  );

  // Wi-Fi 상태
  display.setCursor(
    98,
    0
  );

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {
    display.print(
      "WiFi"
    );
  } else {
    display.print(
      "OFF"
    );
  }

  // 현재 시간
  display.setTextSize(2);

  display.setCursor(
    34,
    13
  );

  display.print(
    timeText
  );

  // 구분선
  display.drawLine(
    0,
    32,
    127,
    32,
    SSD1306_WHITE
  );

  int nextIndex =
    findNextEventIndex();

  display.setTextSize(1);

  if (
    nextIndex >= 0
  ) {
    ScheduleEvent& event =
      scheduleEvents[
        nextIndex
      ];

    int daysUntil =
      getDaysUntil(
        event
      );

    // NEXT D-2
    display.setCursor(
      0,
      35
    );

    display.print(
      "NEXT "
    );

    display.print(
      getDDayText(
        daysUntil
      )
    );

    // 한글 일정 제목
    drawUtf8Text(
      display,
      0,
      46,
      event.title
    );

  } else {

    display.setCursor(
      0,
      36
    );

    display.print(
      "NEXT"
    );

    drawUtf8Text(
      display,
      0,
      46,
      "예정 없음"
    );
  }

  display.display();
}


// --------------------------------------------------
// CALENDAR
// --------------------------------------------------

void showCalendar() {
  struct tm timeinfo;

  if (
    !getLocalTime(
      &timeinfo
    )
  ) {
    showMessage(
      "TIME",
      "NOT AVAILABLE"
    );

    return;
  }

  char dateText[10];

  strftime(
    dateText,
    sizeof(dateText),
    "%m/%d",
    &timeinfo
  );

  int pageCount =
    getCalendarPageCount();

  // 일정 개수가 변경되어
  // 현재 페이지가 범위를 벗어난 경우
  if (
    calendarPage >=
    pageCount
  ) {
    calendarPage = 0;
  }

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );


  // -------------------------
  // 상단
  // -------------------------

  drawUtf8Text(
    display,
    0,
    0,
    "일정"
  );

  display.setTextSize(1);

  display.setCursor(
    84,
    4
  );

  display.print(
    dateText
  );

  display.drawLine(
    0,
    18,
    127,
    18,
    SSD1306_WHITE
  );


  // -------------------------
  // 일정 목록
  // -------------------------

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

    eventShown = true;

    ScheduleEvent& event =
      scheduleEvents[
        eventIndex
      ];

    int daysUntil =
      getDaysUntil(
        event
      );

    int y =
      21 +
      row * 18;


    // D-Day
    display.setTextSize(1);

    display.setCursor(
      0,
      y + 4
    );

    display.print(
      getDDayText(
        daysUntil
      )
    );


    // 한글 일정 제목
    drawUtf8Text(
      display,
      26,
      y,
      event.title
    );
  }


  if (
    !eventShown
  ) {
    drawUtf8Text(
      display,
      0,
      25,
      "예정 없음"
    );
  }


  // -------------------------
  // 하단 페이지 표시
  // -------------------------

  display.setTextSize(1);

  display.setCursor(
    0,
    56
  );

  display.print("<");

  char pageText[12];

  snprintf(
    pageText,
    sizeof(pageText),
    "%d/%d OK",
    calendarPage + 1,
    pageCount
  );

  display.setCursor(
    45,
    56
  );

  display.print(
    pageText
  );

  display.setCursor(
    122,
    56
  );

  display.print(">");

  display.display();
}


// --------------------------------------------------
// 타이머 시작
// --------------------------------------------------

void startTimer() {
  timerStartedAt =
    millis();

  timerRunning =
    true;

  Serial.println(
    "Timer started"
  );
}


// --------------------------------------------------
// 타이머 남은 시간
// --------------------------------------------------

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


// --------------------------------------------------
// TIMER
// --------------------------------------------------

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

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    0,
    0
  );

  display.println(
    "TIMER"
  );

  display.drawLine(
    0,
    11,
    127,
    11,
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    34,
    22
  );

  display.println(
    timerText
  );

  display.setTextSize(1);

  if (
    timerRunning
  ) {
    display.setCursor(
      38,
      44
    );

    display.println(
      "RUNNING"
    );

  } else {

    display.setCursor(
      34,
      44
    );

    display.println(
      "OK START"
    );
  }

  display.setCursor(
    0,
    56
  );

  display.print(
    "< CAL"
  );

  display.setCursor(
    86,
    56
  );

  display.print(
    "HOME >"
  );

  display.display();
}


// --------------------------------------------------
// 범용 알림 발생
// --------------------------------------------------

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


void checkScheduleReminders() {

  // 이미 다른 알림이 떠 있다면
  // 덮어쓰지 않는다.
  if (
    currentScreen ==
    SCREEN_NOTIFICATION
  ) {
    return;
  }

  for (
    int i = 0;
    i < SCHEDULE_EVENT_COUNT;
    i++
  ) {

    ScheduleEvent& event =
      scheduleEvents[i];

    int daysUntil =
      getDaysUntil(
        event
      );

    uint8_t reminderFlag =
      getReminderFlag(
        daysUntil
      );

    // 오늘은 알림 대상 날짜가 아님
    if (
      reminderFlag == 0
    ) {
      continue;
    }

    // 이미 이 단계의 알림을 보여줌
    if (
      event.reminderFlags &
      reminderFlag
    ) {
      continue;
    }


    // 먼저 완료 처리
    event.reminderFlags |=
      reminderFlag;


    String message =
      getDDayText(
        daysUntil
      );

    message += " ";
    message += event.title;


    triggerNotification(
      "일정 알림",
      message
    );

    // 동시에 여러 일정이 있어도
    // 한 번에 하나씩만 표시
    return;
  }
}


// --------------------------------------------------
// 알림 화면
// --------------------------------------------------

void showNotification() {
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );


  // 상단
  drawUtf8Text(
    display,
    0,
    0,
    notificationTitle
  );

  display.drawLine(
    0,
    18,
    127,
    18,
    SSD1306_WHITE
  );


  // 알림 내용
  drawUtf8Text(
    display,
    0,
    22,
    notificationMessage
  );


  // 하단 안내
  display.setTextSize(1);

  display.setCursor(
    30,
    56
  );

  display.print(
    "OK DISMISS"
  );

  display.display();
}


// --------------------------------------------------
// 3버튼 입력
// --------------------------------------------------

ButtonEvent readButtonEvent() {

  for (
    int i = 0;
    i < 3;
    i++
  ) {

    bool reading =
      digitalRead(
        BUTTON_PINS[i]
      );

    if (
      reading !=
      lastButtonReadings[i]
    ) {
      lastButtonChangeAt[i] =
        millis();

      lastButtonReadings[i] =
        reading;
    }

    if (
      millis() -
      lastButtonChangeAt[i]
      >
      BUTTON_DEBOUNCE_MS
    ) {

      if (
        reading !=
        stableButtonStates[i]
      ) {
        stableButtonStates[i] =
          reading;

        if (
          stableButtonStates[i] ==
          LOW
        ) {
          return
            BUTTON_EVENTS[i];
        }
      }
    }
  }

  return BUTTON_NONE;
}


// --------------------------------------------------
// 화면 이동
// --------------------------------------------------

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

    } else if (
      currentScreen ==
      SCREEN_CALENDAR
    ) {
      currentScreen =
        SCREEN_TIMER;

    } else if (
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

    } else if (
      currentScreen ==
      SCREEN_TIMER
    ) {
      currentScreen =
        SCREEN_CALENDAR;

    } else if (
      currentScreen ==
      SCREEN_CALENDAR
    ) {
      currentScreen =
        SCREEN_HOME;
    }
  }
}


// --------------------------------------------------
// 버튼 처리
// --------------------------------------------------

void handleButton(
  ButtonEvent button
) {

  if (
    button ==
    BUTTON_NONE
  ) {
    return;
  }

  // 알림 화면에서는 OK만 사용
  if (
    currentScreen ==
    SCREEN_NOTIFICATION
  ) {

    if (
      button ==
      BUTTON_OK
    ) {
      currentScreen =
        screenBeforeNotification;

      Serial.println(
        "Notification dismissed"
      );
    }

    return;
  }

  // CALENDAR에서 OK → 다음 일정 페이지
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

  // LEFT / RIGHT → 화면 이동
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


// --------------------------------------------------
// setup
// --------------------------------------------------

void setup() {
  Serial.begin(
    115200
  );

  pinMode(
    BUTTON_LEFT_PIN,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_OK_PIN,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_RIGHT_PIN,
    INPUT_PULLUP
  );

  Wire.begin(
    21,
    22
  );

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      SCREEN_ADDRESS
    )
  ) {
    Serial.println(
      "OLED init failed"
    );

    while (
      true
    ) {
      delay(1000);
    }
  }

  showMessage(
    "BOOTING..."
  );

  delay(500);

  connectWiFi();

  syncTime();

  currentScreen =
    SCREEN_HOME;
}


// --------------------------------------------------
// loop
// --------------------------------------------------

void loop() {

  ButtonEvent button =
    readButtonEvent();

  handleButton(
    button
  );

  checkScheduleReminders();


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


  // 현재 화면 출력
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

  delay(20);
}