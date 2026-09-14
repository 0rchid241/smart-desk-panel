#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "wifi_secrets.h"
#include "hangul_renderer.h"

// 로컬 전용 포켓몬 애셋.
// 이 파일은 Git에 올리지 않아도 공개 저장소가 컴파일되도록 조건부 포함한다.
#if __has_include("local_game_assets/pokemon/pikachu_idle_1bit.h")
  #include "local_game_assets/pokemon/pikachu_idle_1bit.h"
  #define HAS_LOCAL_PIKACHU_ASSET 1
#else
  #define HAS_LOCAL_PIKACHU_ASSET 0
#endif


// --------------------------------------------------
// 하드웨어 설정
// --------------------------------------------------

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUTTON_LEFT_PIN 25
#define BUTTON_OK_PIN 26
#define BUTTON_RIGHT_PIN 27

const bool RESET_REMINDER_STORAGE_ON_BOOT =
  false;

// 오프라인 캐시 테스트용. 최종값은 false 유지.
const bool FORCE_OFFLINE_TEST_MODE =
  false;


// OLED 1: Smart Desk / 추후 GAME 그래픽 화면
// SDA = GPIO21, SCL = GPIO22
Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);


// OLED 2: 평상시 Desk Pet / 추후 GAME 텍스트 화면
// SDA = GPIO16, SCL = GPIO17
TwoWire gameWire =
  TwoWire(1);

Adafruit_SSD1306 gameDisplay(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &gameWire,
  OLED_RESET
);


Preferences preferences;
Preferences calendarCachePreferences;

bool preferencesReady = false;
bool calendarCacheReady = false;


// --------------------------------------------------
// 화면 타입
// --------------------------------------------------

enum ScreenMode {
  SCREEN_HOME,
  SCREEN_CALENDAR,
  SCREEN_TIMER,
  SCREEN_NOTIFICATION
};


// --------------------------------------------------
// 일정 데이터 타입
// --------------------------------------------------

struct ScheduleEvent {
  int year;
  int month;
  int day;

  String title;

  uint8_t reminderFlags;
};


// --------------------------------------------------
// 버튼 이벤트 타입
// --------------------------------------------------

enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_LEFT,
  BUTTON_OK,
  BUTTON_RIGHT
};


// --------------------------------------------------
// 사용자 정의 타입을 사용하는 함수 사전 선언
// Arduino 자동 프로토타입 문제 방지
// --------------------------------------------------

uint32_t makeEventHash(
  const ScheduleEvent& event
);

String getReminderStorageKey(
  const ScheduleEvent& event
);

void loadReminderFlags();

void saveReminderFlags(
  const ScheduleEvent& event
);

int getDaysUntil(
  const ScheduleEvent& event
);

String getEventDateText(
  const ScheduleEvent& event
);

bool parseCalendarDate(
  const String& dateText,
  int& year,
  int& month,
  int& day
);

bool parseCalendarPayload(
  const String& payload
);

bool saveCalendarCache(
  const String& payload
);

bool loadCalendarCache();

bool fetchCalendarEvents();

bool syncCalendarEvents();

bool getCurrentTimeInfo(
  struct tm& timeinfo
);

ButtonEvent readButtonEvent();

void moveMainScreen(
  ButtonEvent button
);

void handleButton(
  ButtonEvent button
);


// --------------------------------------------------
// 현재 화면
// --------------------------------------------------

ScreenMode currentScreen =
  SCREEN_HOME;


// --------------------------------------------------
// 날짜 기반 일정 데이터
// --------------------------------------------------

const int MAX_SCHEDULE_EVENTS = 20;

ScheduleEvent scheduleEvents[
  MAX_SCHEDULE_EVENTS
];

int scheduleEventCount = 0;


const int CALENDAR_EVENTS_PER_PAGE =
  2;

int calendarPage =
  0;


// --------------------------------------------------
// 일정 알림 플래그
// --------------------------------------------------

const uint8_t REMINDER_D3 =
  1 << 0;

const uint8_t REMINDER_D1 =
  1 << 1;

const uint8_t REMINDER_DDAY =
  1 << 2;


// --------------------------------------------------
// 알림 상태
// --------------------------------------------------

String notificationTitle =
  "";

String notificationMessage =
  "";

ScreenMode screenBeforeNotification =
  SCREEN_HOME;


// --------------------------------------------------
// 버튼 상태
// --------------------------------------------------

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

bool timerRunning =
  false;

unsigned long timerStartedAt =
  0;

// 아직 테스트용 10초
const unsigned long TIMER_DURATION_MS =
  10000;


// --------------------------------------------------
// Google Calendar 자동 동기화
// --------------------------------------------------

// 최종 자동 동기화 주기: 15분
const unsigned long CALENDAR_SYNC_INTERVAL_MS =
  15UL * 60UL * 1000UL;

unsigned long lastCalendarSyncAt =
  0;


// --------------------------------------------------
// Wi-Fi 오프라인 / 재연결 상태
// --------------------------------------------------

const unsigned long WIFI_CONNECT_TIMEOUT_MS =
  12000;

const unsigned long WIFI_RECONNECT_INTERVAL_MS =
  30000;

unsigned long lastWifiReconnectAt =
  0;

bool wifiWasConnected =
  false;


// --------------------------------------------------
// Desk Pet 상태
// --------------------------------------------------

#if HAS_LOCAL_PIKACHU_ASSET

const uint16_t PIKACHU_IDLE_FRAME_DURATION_MS[
  PIKACHU_IDLE_FRAME_COUNT
] = {
  2000,
  100,
  150,
  150,
  150,
  100
};

int deskPetFrame =
  0;

unsigned long deskPetFrameStartedAt =
  0;

#endif


// --------------------------------------------------
// Desk Pet 화면
// --------------------------------------------------

void drawDeskPet() {

  gameDisplay.clearDisplay();

#if HAS_LOCAL_PIKACHU_ASSET

  const int x =
    (
      SCREEN_WIDTH -
      PIKACHU_IDLE_FRAME_WIDTH
    ) / 2;

  const int y =
    (
      SCREEN_HEIGHT -
      PIKACHU_IDLE_FRAME_HEIGHT
    ) / 2;

  gameDisplay.drawBitmap(
    x,
    y,
    pikachu_idle_frames[
      deskPetFrame
    ],
    PIKACHU_IDLE_FRAME_WIDTH,
    PIKACHU_IDLE_FRAME_HEIGHT,
    SSD1306_WHITE
  );

#else

  // 공개 저장소에는 포켓몬 애셋을 넣지 않으므로
  // 로컬 애셋이 없을 때도 펌웨어 자체는 정상 컴파일된다.
  gameDisplay.setTextColor(
    SSD1306_WHITE
  );

  gameDisplay.setTextSize(1);

  gameDisplay.setCursor(
    0,
    0
  );

  gameDisplay.println(
    "DESK PET"
  );

  gameDisplay.drawLine(
    0,
    12,
    127,
    12,
    SSD1306_WHITE
  );

  gameDisplay.setCursor(
    12,
    27
  );

  gameDisplay.println(
    "LOCAL ASSET"
  );

  gameDisplay.setCursor(
    21,
    40
  );

  gameDisplay.println(
    "NOT FOUND"
  );

#endif

  gameDisplay.display();
}


// --------------------------------------------------
// Desk Pet 애니메이션 갱신
// --------------------------------------------------

void updateDeskPet() {

#if HAS_LOCAL_PIKACHU_ASSET

  unsigned long now =
    millis();

  if (
    now -
      deskPetFrameStartedAt >=
      PIKACHU_IDLE_FRAME_DURATION_MS[
        deskPetFrame
      ]
  ) {

    deskPetFrame =
      (
        deskPetFrame + 1
      ) %
      PIKACHU_IDLE_FRAME_COUNT;

    deskPetFrameStartedAt =
      now;

    drawDeskPet();
  }

#endif
}


// --------------------------------------------------
// 일정 고유 Hash 생성
// --------------------------------------------------

uint32_t makeEventHash(
  const ScheduleEvent& event
) {
  // FNV-1a 32-bit
  uint32_t hash =
    2166136261UL;

  String identity =
    String(event.year) + "-" +
    String(event.month) + "-" +
    String(event.day) + "|" +
    event.title;

  const char* text =
    identity.c_str();

  while (*text) {

    hash ^=
      static_cast<uint8_t>(
        *text++
      );

    hash *=
      16777619UL;
  }

  return hash;
}


// --------------------------------------------------
// 일정 NVS 저장 키 생성
// --------------------------------------------------

String getReminderStorageKey(
  const ScheduleEvent& event
) {
  char buffer[12];

  snprintf(
    buffer,
    sizeof(buffer),
    "r%08lX",
    static_cast<unsigned long>(
      makeEventHash(event)
    )
  );

  return String(buffer);
}


// --------------------------------------------------
// NVS에서 일정 알림 상태 불러오기
// --------------------------------------------------

void loadReminderFlags() {

  if (!preferencesReady) {
    return;
  }

  for (
    int i = 0;
    i < scheduleEventCount;
    i++
  ) {
    String key =
      getReminderStorageKey(
        scheduleEvents[i]
      );

    scheduleEvents[i].reminderFlags =
      preferences.getUChar(
        key.c_str(),
        0
      );

    Serial.print(
      "Reminder load: "
    );

    Serial.print(
      scheduleEvents[i].title
    );

    Serial.print(
      " = "
    );

    Serial.println(
      scheduleEvents[i].reminderFlags
    );
  }
}


// --------------------------------------------------
// NVS에 일정 알림 상태 저장
// --------------------------------------------------

void saveReminderFlags(
  const ScheduleEvent& event
) {

  if (!preferencesReady) {
    return;
  }

  String key =
    getReminderStorageKey(
      event
    );

  preferences.putUChar(
    key.c_str(),
    event.reminderFlags
  );

  Serial.print(
    "Reminder saved: "
  );

  Serial.print(
    event.title
  );

  Serial.print(
    " = "
  );

  Serial.println(
    event.reminderFlags
  );
}


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


// --------------------------------------------------
// NTP 시간 동기화
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
// 현재 시간이 유효한지 확인
// --------------------------------------------------

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


// --------------------------------------------------
// Calendar JSON → 일정 데이터 적용
// --------------------------------------------------

bool parseCalendarPayload(
  const String& payload
) {

  JsonDocument document;

  DeserializationError error =
    deserializeJson(
      document,
      payload
    );

  if (error) {
    Serial.print(
      "Calendar JSON error: "
    );

    Serial.println(
      error.c_str()
    );

    return false;
  }

  bool ok =
    document["ok"] | false;

  if (!ok) {
    Serial.println(
      "Calendar API returned ok=false"
    );

    return false;
  }

  JsonArray events =
    document["events"]
      .as<JsonArray>();

  // JSON 전체가 정상임을 확인한 뒤에만
  // 현재 일정 목록을 교체한다.
  scheduleEventCount =
    0;

  for (
    JsonObject item : events
  ) {

    if (
      scheduleEventCount >=
      MAX_SCHEDULE_EVENTS
    ) {
      Serial.println(
        "Calendar event limit reached"
      );

      break;
    }

    const char* title =
      item["title"] | "";

    const char* date =
      item["date"] | "";

    int year;
    int month;
    int day;

    if (
      !parseCalendarDate(
        String(date),
        year,
        month,
        day
      )
    ) {
      Serial.print(
        "Invalid calendar date: "
      );

      Serial.println(
        date
      );

      continue;
    }

    ScheduleEvent& event =
      scheduleEvents[
        scheduleEventCount
      ];

    event.year =
      year;

    event.month =
      month;

    event.day =
      day;

    event.title =
      String(title);

    event.reminderFlags =
      0;

    scheduleEventCount++;

    Serial.print(
      "Calendar event: "
    );

    Serial.print(
      date
    );

    Serial.print(
      " / "
    );

    Serial.println(
      title
    );
  }

  Serial.print(
    "Calendar loaded: "
  );

  Serial.print(
    scheduleEventCount
  );

  Serial.println(
    " events"
  );

  return true;
}


// --------------------------------------------------
// 마지막 Calendar JSON을 NVS에 저장
// --------------------------------------------------

bool saveCalendarCache(
  const String& payload
) {

  if (!calendarCacheReady) {
    return false;
  }

  size_t written =
    calendarCachePreferences.putString(
      "json",
      payload
    );

  if (written == 0) {
    Serial.println(
      "Calendar cache save failed"
    );

    return false;
  }

  Serial.print(
    "Calendar cache saved: "
  );

  Serial.print(
    written
  );

  Serial.println(
    " bytes"
  );

  return true;
}


// --------------------------------------------------
// NVS에서 마지막 Calendar 일정 복구
// --------------------------------------------------

bool loadCalendarCache() {

  if (!calendarCacheReady) {
    return false;
  }

  String payload =
    calendarCachePreferences.getString(
      "json",
      ""
    );

  if (
    payload.length() == 0
  ) {
    Serial.println(
      "Calendar cache empty"
    );

    return false;
  }

  Serial.println(
    "Calendar cache found"
  );

  if (
    !parseCalendarPayload(
      payload
    )
  ) {
    Serial.println(
      "Calendar cache invalid"
    );

    return false;
  }

  if (
    preferencesReady
  ) {
    loadReminderFlags();
  }

  Serial.println(
    "Calendar cache loaded"
  );

  return true;
}


// --------------------------------------------------
// Google Calendar 일정 가져오기
// --------------------------------------------------

bool fetchCalendarEvents() {

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    Serial.println(
      "Calendar: Wi-Fi not connected"
    );

    return false;
  }

  Serial.println();
  Serial.println(
    "Calendar fetch start"
  );

  WiFiClientSecure client;

  // 현재 단계에서는 인증서 검증을 생략한다.
  client.setInsecure();

  HTTPClient http;

  http.setFollowRedirects(
    HTTPC_STRICT_FOLLOW_REDIRECTS
  );

  if (
    !http.begin(
      client,
      CALENDAR_API_URL
    )
  ) {
    Serial.println(
      "Calendar: HTTP begin failed"
    );

    return false;
  }

  int httpCode =
    http.GET();

  if (
    httpCode !=
    HTTP_CODE_OK
  ) {
    Serial.print(
      "Calendar HTTP error: "
    );

    Serial.println(
      httpCode
    );

    http.end();

    return false;
  }

  String payload =
    http.getString();

  http.end();

  if (
    !parseCalendarPayload(
      payload
    )
  ) {
    return false;
  }

  // 정상 데이터만 캐시에 저장한다.
  saveCalendarCache(
    payload
  );

  return true;
}


// --------------------------------------------------
// Google Calendar 동기화
// --------------------------------------------------

bool syncCalendarEvents() {

  Serial.println();
  Serial.println(
    "Calendar sync start"
  );

  bool success =
    fetchCalendarEvents();

  if (!success) {
    Serial.println(
      "Calendar sync failed - keeping current/cache data"
    );

    return false;
  }

  // 새 일정 객체에 기존 알림 기록을 다시 적용한다.
  if (
    preferencesReady
  ) {
    loadReminderFlags();
  }

  Serial.println(
    "Calendar sync success"
  );

  return true;
}


// --------------------------------------------------
// 윤년 검사
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


// --------------------------------------------------
// 날짜 → 일련번호
// --------------------------------------------------

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
    DAYS_BEFORE_MONTH[
      month
    ];

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
// YYYY-MM-DD 문자열 파싱
// --------------------------------------------------

bool parseCalendarDate(
  const String& dateText,
  int& year,
  int& month,
  int& day
) {
  if (
    dateText.length() != 10
  ) {
    return false;
  }

  if (
    dateText.charAt(4) != '-' ||
    dateText.charAt(7) != '-'
  ) {
    return false;
  }

  year =
    dateText.substring(
      0,
      4
    ).toInt();

  month =
    dateText.substring(
      5,
      7
    ).toInt();

  day =
    dateText.substring(
      8,
      10
    ).toInt();


  if (
    year < 2000 ||
    month < 1 ||
    month > 12 ||
    day < 1 ||
    day > 31
  ) {
    return false;
  }

  return true;
}


// --------------------------------------------------
// 일정까지 남은 날짜 계산
// --------------------------------------------------

int getDaysUntil(
  const ScheduleEvent& event
) {
  struct tm timeinfo;

  if (
    !getCurrentTimeInfo(
      timeinfo
    )
  ) {
    return 99999;
  }

  int currentYear =
    timeinfo.tm_year +
    1900;

  int currentMonth =
    timeinfo.tm_mon +
    1;

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
      eventDay -
      today
    );
}


// --------------------------------------------------
// 앞으로 표시할 일정 개수
// --------------------------------------------------

int countUpcomingEvents() {

  struct tm timeinfo;
  bool timeAvailable =
    getCurrentTimeInfo(
      timeinfo
    );

  // 시간을 모르는 오프라인 부팅에서는
  // 캐시에 들어 있던 일정을 모두 보여준다.
  if (!timeAvailable) {
    return scheduleEventCount;
  }

  int count =
    0;

  for (
    int i = 0;
    i < scheduleEventCount;
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

  struct tm timeinfo;
  bool timeAvailable =
    getCurrentTimeInfo(
      timeinfo
    );

  for (
    int candidateIndex = 0;
    candidateIndex <
      scheduleEventCount;
    candidateIndex++
  ) {

    long candidateValue;

    if (timeAvailable) {
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

      candidateValue =
        candidateDays;

    } else {
      candidateValue =
        dateToDayNumber(
          scheduleEvents[candidateIndex].year,
          scheduleEvents[candidateIndex].month,
          scheduleEvents[candidateIndex].day
        );
    }

    int rank =
      0;

    for (
      int otherIndex = 0;
      otherIndex <
        scheduleEventCount;
      otherIndex++
    ) {

      if (
        otherIndex ==
        candidateIndex
      ) {
        continue;
      }

      long otherValue;

      if (timeAvailable) {
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

        otherValue =
          otherDays;

      } else {
        otherValue =
          dateToDayNumber(
            scheduleEvents[otherIndex].year,
            scheduleEvents[otherIndex].month,
            scheduleEvents[otherIndex].day
          );
      }

      if (
        otherValue <
        candidateValue
      ) {
        rank++;
      }

      else if (
        otherValue ==
          candidateValue &&
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
      return candidateIndex;
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
// D-Day 문자열
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


// --------------------------------------------------
// 날짜에 해당하는 알림 플래그
// --------------------------------------------------

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
// 일정 날짜 표시 문자열
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

  return String(
    buffer
  );
}


// --------------------------------------------------
// HOME
// --------------------------------------------------

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

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    0,
    0
  );

  display.print(
    dateText
  );

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

  display.setTextSize(2);

  display.setCursor(
    34,
    13
  );

  display.print(
    timeText
  );

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

    display.setCursor(
      0,
      35
    );

    display.print(
      "NEXT "
    );

    if (timeAvailable) {
      display.print(
        getDDayText(
          getDaysUntil(
            event
          )
        )
      );

    } else {
      display.print(
        getEventDateText(
          event
        )
      );
    }

    drawScrollingUtf8Text(
      display,
      0,
      46,
      128,
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

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

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

    ScheduleEvent& event =
      scheduleEvents[
        eventIndex
      ];

    int y =
      21 +
      row * 18;

    display.setTextSize(1);

    display.setCursor(
      0,
      y + 4
    );

    if (timeAvailable) {
      display.print(
        getDDayText(
          getDaysUntil(
            event
          )
        )
      );

    } else {
      display.print(
        getEventDateText(
          event
        )
      );
    }

    drawScrollingUtf8Text(
      display,
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
      display,
      0,
      25,
      "예정 없음"
    );
  }

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


// --------------------------------------------------
// 일정 알림 검사
// --------------------------------------------------

void checkScheduleReminders() {

  struct tm timeinfo;

  // 현재 날짜를 신뢰할 수 없으면
  // 날짜 기반 알림은 발생시키지 않는다.
  if (
    !getCurrentTimeInfo(
      timeinfo
    )
  ) {
    return;
  }

  // 다른 알림이 표시 중이면
  // 새 알림으로 덮어쓰지 않는다.
  if (
    currentScreen ==
    SCREEN_NOTIFICATION
  ) {
    return;
  }


  for (
    int i = 0;
    i < scheduleEventCount;
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


    // D-3 / D-1 / D-DAY가 아님
    if (
      reminderFlag == 0
    ) {
      continue;
    }


    // 이미 해당 단계 알림 완료
    if (
      event.reminderFlags &
      reminderFlag
    ) {
      continue;
    }


    // 알림 완료 처리
    event.reminderFlags |=
      reminderFlag;


    // NVS에도 즉시 저장
    saveReminderFlags(
      event
    );


    String message =
      getDDayText(
        daysUntil
      );

    message += " ";

    message +=
      event.title;


    triggerNotification(
      "일정 알림",
      message
    );


    // 한 번에 알림 하나만 출력
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


  // 알림 제목
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
  drawScrollingUtf8Text(
      display,
      0,
      24,
      128,
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


  // 알림에서는 OK만 사용
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

  // -------------------------
  // OLED 1 / OLED 2 I2C 버스 시작
  // -------------------------

  Wire.begin(
    21,
    22
  );

  gameWire.begin(
    16,
    17
  );


  // -------------------------
  // OLED 1: Smart Desk
  // -------------------------

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      SCREEN_ADDRESS,
      true,
      false
    )
  ) {
    Serial.println(
      "Desk OLED init failed"
    );

    while (true) {
      delay(1000);
    }
  }


  // -------------------------
  // OLED 2: Desk Pet / Game
  // -------------------------

  if (
    !gameDisplay.begin(
      SSD1306_SWITCHCAPVCC,
      SCREEN_ADDRESS,
      true,
      false
    )
  ) {
    Serial.println(
      "Game OLED init failed"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println(
    "Both OLEDs initialized"
  );


  showMessage(
    "BOOTING..."
  );


  // 두 번째 화면은 네트워크 초기화 중에도
  // Desk Pet 화면을 먼저 보여준다.
#if HAS_LOCAL_PIKACHU_ASSET
  deskPetFrame = 0;
  deskPetFrameStartedAt =
    millis();
#endif

  drawDeskPet();

  delay(500);


  // -------------------------
  // NVS 준비
  // -------------------------

  preferencesReady =
    preferences.begin(
      "schedule",
      false
    );

  if (
    !preferencesReady
  ) {
    Serial.println(
      "Preferences init failed"
    );

  } else {
    Serial.println(
      "Preferences initialized"
    );

    if (
      RESET_REMINDER_STORAGE_ON_BOOT
    ) {
      preferences.clear();

      Serial.println(
        "Reminder storage cleared"
      );
    }
  }


  calendarCacheReady =
    calendarCachePreferences.begin(
      "calcache",
      false
    );

  if (
    !calendarCacheReady
  ) {
    Serial.println(
      "Calendar cache init failed"
    );

  } else {
    Serial.println(
      "Calendar cache initialized"
    );
  }


  // -------------------------
  // 마지막 성공 일정 먼저 복구
  // -------------------------

  loadCalendarCache();


  // -------------------------
  // 네트워크 연결
  // -------------------------

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

    // 최신 Google Calendar로 캐시/화면 갱신
    syncCalendarEvents();
  }

  lastCalendarSyncAt =
    millis();

  lastWifiReconnectAt =
    millis();

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


  // --------------------------------------------------
  // Wi-Fi 재연결
  // --------------------------------------------------

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

    syncCalendarEvents();

    lastCalendarSyncAt =
      millis();
  }

  wifiWasConnected =
    wifiConnected;


  // --------------------------------------------------
  // Google Calendar 주기적 동기화
  // --------------------------------------------------

  if (
    millis() -
      lastCalendarSyncAt >=
      CALENDAR_SYNC_INTERVAL_MS
  ) {

    lastCalendarSyncAt =
      millis();

    if (
      WiFi.status() ==
      WL_CONNECTED
    ) {
      syncCalendarEvents();

    } else {
      Serial.println(
        "Calendar sync skipped: Wi-Fi offline"
      );
    }
  }


  // 일정 D-3 / D-1 / D-DAY 검사
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


  // 두 번째 OLED의 Desk Pet은
  // Smart Desk 화면과 독립적으로 계속 움직인다.
  updateDeskPet();


  delay(20);
}
