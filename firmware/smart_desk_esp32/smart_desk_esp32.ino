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
  SCREEN_TIMER,
  SCREEN_NOTIFICATION
};

ScreenMode currentScreen = SCREEN_HOME;


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

const unsigned long BUTTON_DEBOUNCE_MS = 30;


// --------------------------------------------------
// 타이머 상태
// --------------------------------------------------

bool timerRunning = false;

unsigned long timerStartedAt = 0;

// 아직 테스트용 10초
const unsigned long TIMER_DURATION_MS = 10000;


// --------------------------------------------------
// 일반 메시지
// --------------------------------------------------

void showMessage(
  const char* line1,
  const char* line2 = ""
) {
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
// Wi-Fi
// --------------------------------------------------

void connectWiFi() {
  showMessage("Wi-Fi", "Connecting...");

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  while (
    WiFi.status() != WL_CONNECTED
  ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.println(WiFi.localIP());

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
// HOME
// --------------------------------------------------

void showHome() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
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

  display.setCursor(0, 0);
  display.print(dateText);

  display.setCursor(98, 0);

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {
    display.print("WiFi");
  } else {
    display.print("OFF");
  }

  display.setTextSize(2);

  display.setCursor(34, 13);
  display.print(timeText);

  display.drawLine(
    0,
    32,
    127,
    32,
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(0, 37);
  display.println("NEXT");

  display.setCursor(0, 49);
  display.println("No schedule");

  // 현재 좌우 버튼으로 TIMER 이동 가능
  display.setCursor(74, 56);
  display.print("<> TIMER");

  display.display();
}


// --------------------------------------------------
// 타이머 시작
// --------------------------------------------------

void startTimer() {
  timerStartedAt = millis();

  timerRunning = true;

  Serial.println(
    "Timer started"
  );
}


// --------------------------------------------------
// 타이머 남은 시간
// --------------------------------------------------

unsigned long getTimerRemainingSeconds() {

  // 아직 시작하지 않았다면
  // 전체 설정 시간을 보여준다.
  if (!timerRunning) {
    return
      TIMER_DURATION_MS / 1000;
  }

  unsigned long elapsed =
    millis() - timerStartedAt;

  if (
    elapsed >= TIMER_DURATION_MS
  ) {
    return 0;
  }

  unsigned long remainingMs =
    TIMER_DURATION_MS - elapsed;

  return
    (remainingMs + 999) / 1000;
}


// --------------------------------------------------
// TIMER 화면
// --------------------------------------------------

void showTimer() {
  unsigned long remainingSeconds =
    getTimerRemainingSeconds();

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
  display.setTextColor(
    SSD1306_WHITE
  );

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

  display.setCursor(34, 22);
  display.println(timerText);

  display.setTextSize(1);

  if (timerRunning) {
    display.setCursor(38, 44);
    display.println("RUNNING");

    display.setCursor(0, 56);
    display.print("< HOME");
  } else {
    display.setCursor(34, 44);
    display.println("OK START");

    display.setCursor(0, 56);
    display.print("< HOME");
  }

  display.display();
}


// --------------------------------------------------
// 알림
// --------------------------------------------------

void showNotification() {
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println(
    "! NOTIFICATION"
  );

  display.drawLine(
    0,
    11,
    127,
    11,
    SSD1306_WHITE
  );

  display.setCursor(0, 22);
  display.println(
    "TIMER FINISHED"
  );

  display.setCursor(0, 40);
  display.println(
    "Press OK"
  );

  display.setCursor(0, 52);
  display.println(
    "to dismiss"
  );

  display.display();
}


// --------------------------------------------------
// 3버튼 입력
// --------------------------------------------------

ButtonEvent readButtonEvent() {

  for (int i = 0; i < 3; i++) {

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
          stableButtonStates[i]
          == LOW
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
// 버튼 처리
// --------------------------------------------------

void handleButton(
  ButtonEvent button
) {

  if (button == BUTTON_NONE) {
    return;
  }


  // 알림이 떠 있을 때는
  // OK만 동작
  if (
    currentScreen ==
    SCREEN_NOTIFICATION
  ) {

    if (button == BUTTON_OK) {
      currentScreen =
        SCREEN_HOME;

      Serial.println(
        "Notification dismissed"
      );
    }

    return;
  }


  // HOME
  if (
    currentScreen ==
    SCREEN_HOME
  ) {

    if (
      button == BUTTON_LEFT ||
      button == BUTTON_RIGHT
    ) {
      currentScreen =
        SCREEN_TIMER;

      Serial.println(
        "Screen: TIMER"
      );
    }

    return;
  }


  // TIMER
  if (
    currentScreen ==
    SCREEN_TIMER
  ) {

    if (
      button == BUTTON_LEFT ||
      button == BUTTON_RIGHT
    ) {
      currentScreen =
        SCREEN_HOME;

      Serial.println(
        "Screen: HOME"
      );

      return;
    }

    if (
      button == BUTTON_OK &&
      !timerRunning
    ) {
      startTimer();

      return;
    }
  }
}


// --------------------------------------------------
// setup
// --------------------------------------------------

void setup() {
  Serial.begin(115200);

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

  Wire.begin(21, 22);

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      SCREEN_ADDRESS
    )
  ) {

    Serial.println(
      "OLED init failed"
    );

    while (true) {
    }
  }

  showMessage("BOOTING...");

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

  handleButton(button);


  // 타이머는 현재 보고 있는 화면과
  // 상관없이 계속 진행한다.
  if (
    timerRunning &&
    millis() - timerStartedAt >=
    TIMER_DURATION_MS
  ) {

    timerRunning = false;

    currentScreen =
      SCREEN_NOTIFICATION;

    Serial.println(
      "Timer finished"
    );
  }


  if (
    currentScreen ==
    SCREEN_HOME
  ) {

    showHome();

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