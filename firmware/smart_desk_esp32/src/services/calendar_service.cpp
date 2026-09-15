#include "calendar_service.h"
#include "network_time.h"
#include "../core/app_config.h"
#include "../../wifi_secrets.h"
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
using namespace AppConfig;
using NetworkTime::getCurrentTimeInfo;
namespace CalendarService {
namespace {
Preferences preferences;
Preferences calendarCachePreferences;

bool preferencesReady = false;
bool calendarCacheReady = false;
ScheduleEvent scheduleEvents[
  MAX_SCHEDULE_EVENTS
];

int scheduleEventCount = 0;
unsigned long lastCalendarSyncAt =
  0;

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
bool parseCalendarPayload(
  const String& payload
);
bool saveCalendarCache(
  const String& payload
);
bool loadCalendarCache();
bool fetchCalendarEvents();
bool isLeapYear(
  int year
);
long dateToDayNumber(
  int year,
  int month,
  int day
);
bool parseCalendarDate(
  const String& dateText,
  int& year,
  int& month,
  int& day
);
int countUpcomingEvents();
uint8_t getReminderFlag(
  int daysUntil
);

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
} // namespace

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

int findNextEventIndex() {

  return
    findUpcomingEventIndexByOrder(
      0
    );
}

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

void init() {
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


}

void startSyncClock() { lastCalendarSyncAt = millis(); }

void update(bool reconnected) {
  if (reconnected) {
    syncCalendarEvents();
    lastCalendarSyncAt = millis();
  }
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


}

void checkScheduleReminders(bool notificationActive, NotificationHandler notify) {

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
    notificationActive
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


    notify(
      "일정 알림",
      message
    );


    // 한 번에 알림 하나만 출력
    return;
  }
}

const ScheduleEvent& getEvent(int index) { return scheduleEvents[index]; }
} // namespace CalendarService
