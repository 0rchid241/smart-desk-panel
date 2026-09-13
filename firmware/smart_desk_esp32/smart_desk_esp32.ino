#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

#include "wifi_secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUTTON_PIN 25

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
  SCREEN_TIMER,
  SCREEN_NOTIFICATION
};

ScreenMode currentScreen = SCREEN_HOME;


// --------------------------------------------------
// 타이머 상태
// --------------------------------------------------

bool timerRunning = false;

unsigned long timerStartedAt = 0;

// 테스트용 10초
const unsigned long TIMER_DURATION_MS = 10000;


// --------------------------------------------------
// 버튼 디바운싱
// --------------------------------------------------

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastButtonChangeAt = 0;

const unsigned long BUTTON_DEBOUNCE_MS = 30;


// --------------------------------------------------
// 일반 메시지 화면
// --------------------------------------------------

void showMessage(const char* line1, const char* line2 = "") {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("SMART DESK");

  display.setCursor(0, 20);
  display.println(line1);

  display.setCursor(0, 35);
  display.println(line2);

  display.display();
}


// --------------------------------------------------
// Wi-Fi 연결
// --------------------------------------------------

void connectWiFi() {
  showMessage("Wi-Fi", "Connecting...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.println(WiFi.localIP());

  showMessage("Wi-Fi", "CONNECTED");

  delay(1000);
}


// --------------------------------------------------
// NTP 시간 동기화
// --------------------------------------------------

void syncTime() {
  showMessage("TIME", "Syncing...");

  // 대한민국 UTC+9
  configTime(
    9 * 3600,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("Time sync failed");

    showMessage("TIME", "SYNC FAILED");

    return;
  }

  Serial.println("Time sync success");
}


// --------------------------------------------------
// HOME 화면
// --------------------------------------------------

void showHome() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    showMessage("TIME", "NOT AVAILABLE");

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

  display.setTextColor(SSD1306_WHITE);

  // 날짜
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(dateText);

  // Wi-Fi 상태
  display.setCursor(98, 0);

  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi");
  } else {
    display.print("OFF");
  }

  // 현재 시간
  display.setTextSize(2);

  display.setCursor(34, 13);
  display.print(timeText);

  // 구분선
  display.drawLine(
    0,
    32,
    127,
    32,
    SSD1306_WHITE
  );

  // 다음 일정
  display.setTextSize(1);

  display.setCursor(0, 37);
  display.println("NEXT");

  display.setCursor(0, 51);
  display.println("No schedule");

  display.display();
}


// --------------------------------------------------
// 타이머 시작
// --------------------------------------------------

void startTimer() {
  timerStartedAt = millis();

  timerRunning = true;

  currentScreen = SCREEN_TIMER;

  Serial.println("Timer started");
}


// --------------------------------------------------
// 타이머 화면
// --------------------------------------------------

void showTimer() {
  unsigned long elapsed =
    millis() - timerStartedAt;

  unsigned long remainingMs;

  if (elapsed >= TIMER_DURATION_MS) {
    remainingMs = 0;
  } else {
    remainingMs =
      TIMER_DURATION_MS - elapsed;
  }

  // 999를 더해서 초 표시가 너무 빨리 줄어드는 것을 방지
  unsigned long remainingSeconds =
    (remainingMs + 999) / 1000;

  unsigned int minutes =
    remainingSeconds / 60;

  unsigned int seconds =
    remainingSeconds % 60;


  char timerText[10];

  snprintf(
    timerText,
    sizeof(timerText),
    "%02u:%02u",
    minutes,
    seconds
  );


  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("TIMER");

  display.drawLine(
    0,
    11,
    127,
    11,
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(34, 23);
  display.println(timerText);

  display.setTextSize(1);

  display.setCursor(29, 51);
  display.println("FOCUS");

  display.display();
}


// --------------------------------------------------
// 알림 화면
// --------------------------------------------------

void showNotification() {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("! NOTIFICATION");

  display.drawLine(
    0,
    11,
    127,
    11,
    SSD1306_WHITE
  );

  display.setCursor(0, 22);
  display.println("TIMER FINISHED");

  display.setCursor(0, 40);
  display.println("Press button");

  display.setCursor(0, 52);
  display.println("to dismiss");

  display.display();
}


// --------------------------------------------------
// 버튼 1회 입력 감지
// --------------------------------------------------

bool buttonWasPressed() {
  bool reading =
    digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastButtonChangeAt = millis();

    lastButtonReading = reading;
  }

  if (
    millis() - lastButtonChangeAt >
    BUTTON_DEBOUNCE_MS
  ) {

    if (reading != stableButtonState) {
      stableButtonState = reading;

      if (stableButtonState == LOW) {
        return true;
      }
    }
  }

  return false;
}


// --------------------------------------------------
// 초기 설정
// --------------------------------------------------

void setup() {
  Serial.begin(115200);

  pinMode(
    BUTTON_PIN,
    INPUT_PULLUP
  );

  Wire.begin(21, 22);

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      SCREEN_ADDRESS
    )
  ) {

    Serial.println("OLED init failed");

    while (true) {
    }
  }

  showMessage("BOOTING...");

  delay(500);

  connectWiFi();

  syncTime();

  currentScreen = SCREEN_HOME;
}


// --------------------------------------------------
// 메인 반복
// --------------------------------------------------

void loop() {

  // --------------------------------
  // 버튼 입력
  // --------------------------------

  if (buttonWasPressed()) {

    // HOME에서 버튼 → 타이머 시작
    if (currentScreen == SCREEN_HOME) {
      startTimer();
    }

    // 알림에서 버튼 → HOME 복귀
    else if (
      currentScreen ==
      SCREEN_NOTIFICATION
    ) {

      currentScreen = SCREEN_HOME;

      Serial.println(
        "Notification dismissed"
      );
    }
  }


  // --------------------------------
  // 타이머 종료 확인
  // --------------------------------

  if (
    timerRunning &&
    millis() - timerStartedAt >=
    TIMER_DURATION_MS
  ) {

    timerRunning = false;

    currentScreen =
      SCREEN_NOTIFICATION;

    Serial.println("Timer finished");
  }


  // --------------------------------
  // 화면 출력
  // --------------------------------

  if (currentScreen == SCREEN_HOME) {

    showHome();

  } else if (
    currentScreen == SCREEN_TIMER
  ) {

    showTimer();

  } else if (
    currentScreen ==
    SCREEN_NOTIFICATION
  ) {

    showNotification();
  }


  delay(50);
}