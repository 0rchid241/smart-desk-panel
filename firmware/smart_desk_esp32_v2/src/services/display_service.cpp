#include "display_service.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "lin_server_service.h"
#include "network_service.h"

namespace DisplayService {
namespace {

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;

constexpr int OLED_RESET = -1;
constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr int OLED_SDA_PIN = 21;
constexpr int OLED_SCL_PIN = 22;

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET);

bool ready = false;

bool lastWifiConnected = false;
bool lastLinAvailable = false;
bool hasRenderedStatus = false;

void renderStatus(
    bool wifiConnected,
    bool linAvailable) {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);

  display.setCursor(0, 0);
  display.println("DeskMon v2");

  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  display.setCursor(0, 24);
  display.print("WiFi: ");
  display.println(
      wifiConnected ? "ONLINE" : "OFFLINE");

  display.setCursor(0, 40);
  display.print("Lin : ");
  display.println(
      linAvailable ? "ONLINE" : "OFFLINE");

  display.display();
}

}  // namespace

void init() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  if (!display.begin(
          SSD1306_SWITCHCAPVCC,
          OLED_ADDRESS)) {
    Serial.println("OLED1 init failed");
    return;
  }

  ready = true;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);

  display.setCursor(0, 0);
  display.println("DeskMon v2");
  display.println();
  display.println("Booting...");

  display.display();

  Serial.println("OLED1 initialized");
}

void update() {
  if (!ready) {
    return;
  }

  const bool wifiConnected =
      NetworkService::isWifiConnected();

  const bool linAvailable =
      LinServerService::isAvailable();

  if (!hasRenderedStatus ||
      wifiConnected != lastWifiConnected ||
      linAvailable != lastLinAvailable) {
    renderStatus(
        wifiConnected,
        linAvailable);

    lastWifiConnected = wifiConnected;
    lastLinAvailable = linAvailable;
    hasRenderedStatus = true;
  }
}

bool isReady() {
  return ready;
}

}  // namespace DisplayService
