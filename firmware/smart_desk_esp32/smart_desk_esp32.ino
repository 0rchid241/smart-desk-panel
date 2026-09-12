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

void syncTime() {
  showMessage("TIME", "Syncing...");

  // 대한민국 = UTC+9, 서머타임 없음
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

void showClock() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    showMessage("TIME", "NOT AVAILABLE");
    return;
  }

  char dateText[20];
  char timeText[20];

  strftime(dateText, sizeof(dateText), "%Y-%m-%d", &timeinfo);
  strftime(timeText, sizeof(timeText), "%H:%M:%S", &timeinfo);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SMART DESK");

  display.setCursor(0, 20);
  display.println(dateText);

  display.setTextSize(2);
  display.setCursor(0, 38);
  display.println(timeText);

  display.display();
}

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
}

void loop() {
  showClock();
  delay(1000);
}