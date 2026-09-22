#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "wifi_secrets.h"

static const char* SERVER_URL =
    "http://192.168.35.187:8000/api/device/message";

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void showMessage(const char* line1,
                 const char* line2 = "",
                 const char* line3 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(line1);
  display.println();
  display.println(line2);
  display.println(line3);
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    return;
  }

  showMessage("DeskMon HTTP Test", "Connecting...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long startedAt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < 15000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection failed");
    showMessage("Wi-Fi", "OFFLINE");
    return;
  }

  Serial.println("Wi-Fi connected");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  if (!http.begin(SERVER_URL)) {
    Serial.println("HTTP begin failed");
    showMessage("LIN SERVER", "HTTP BEGIN FAIL");
    return;
  }

  const int statusCode = http.GET();

  Serial.print("HTTP status: ");
  Serial.println(statusCode);

  if (statusCode != 200) {
    showMessage("LIN SERVER", "HTTP ERROR");
    http.end();
    return;
  }

  const String body = http.getString();

  Serial.print("Body: ");
  Serial.println(body);

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, body);

  if (error) {
    Serial.println("JSON parse failed");
    showMessage("LIN SERVER", "JSON ERROR");
    http.end();
    return;
  }

  const char* message = doc["message"] | "(no message)";

  showMessage(
      "LIN SERVER",
      "HTTP 200",
      message
  );

  http.end();
}

void loop() {
  delay(1000);
}
