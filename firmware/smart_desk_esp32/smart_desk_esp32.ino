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

// 현재 어떤 화면을 보여줄지 나타내는 상태
enum ScreenMode {
  SCREEN_HOME,
  SCREEN_NOTIFICATION
};

ScreenMode currentScreen = SCREEN_HOME;

// 테스트 알림용
unsigned long homeStartedAt = 0;
bool testNotificationShown = false;

// 버튼 디바운싱용
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

  // 대한민국 = UTC+9
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

  strftime(dateText, sizeof(dateText), "%m/%d %a", &timeinfo);
  strftime(timeText, sizeof(timeText), "%H:%M", &timeinfo);

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
  display.drawLine(0, 32, 127, 32, SSD1306_WHITE);

  // 다음 일정
  display.setTextSize(1);
  display.setCursor(0, 37);
  display.println("NEXT");

  display.setCursor(0, 51);
  display.println("No schedule");

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

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  display.setCursor(0, 22);
  display.println("Test reminder");

  display.setCursor(0, 40);
  display.println("Press button");

  display.setCursor(0, 52);
  display.println("to dismiss");

  display.display();
}


// --------------------------------------------------
// 버튼을 '한 번 눌렀는지' 확인
// --------------------------------------------------

bool buttonWasPressed() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastButtonChangeAt = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastButtonChangeAt > BUTTON_DEBOUNCE_MS) {
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

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED init failed");

    while (true) {
    }
  }

  showMessage("BOOTING...");
  delay(500);

  connectWiFi();
  syncTime();

  currentScreen = SCREEN_HOME;

  // 여기부터 15초를 센다.
  homeStartedAt = millis();
}


// --------------------------------------------------
// 반복 실행
// --------------------------------------------------

void loop() {
  // HOME에 진입한 뒤 15초가 지나면
  // 테스트 알림을 딱 한 번 발생시킨다.
  if (
    !testNotificationShown &&
    millis() - homeStartedAt >= 15000
  ) {
    currentScreen = SCREEN_NOTIFICATION;
    testNotificationShown = true;

    Serial.println("Test notification triggered");
  }

  // 버튼을 눌렀다면
  if (buttonWasPressed()) {
    // 알림 화면일 때만 알림을 닫는다.
    if (currentScreen == SCREEN_NOTIFICATION) {
      currentScreen = SCREEN_HOME;

      Serial.println("Notification dismissed");
    }
  }

  // 현재 상태에 맞는 화면 표시
  if (currentScreen == SCREEN_HOME) {
    showHome();
  } else if (currentScreen == SCREEN_NOTIFICATION) {
    showNotification();
  }

  delay(50);
}