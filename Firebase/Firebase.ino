#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"
#include "secrets.h" 

// -------- OLED --------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------- REED SWITCH --------
#define REED_PIN 4

int reedState = HIGH;
int lastReedState = HIGH;

int openCount = 0;
int closeCount = 0;

// -------- FIREBASE --------
String projectId = "esp32-demo-project-47c9e";

// -------- TIME --------
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// -------- TIMERS --------
unsigned long lastDisplayUpdate = 0;
unsigned long lastTrigger = 0;  // debounce

void setup() {
  Serial.begin(115200);

  pinMode(REED_PIN, INPUT_PULLUP);
  Wire.begin(21, 22);

  // OLED Init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (1);
  }

  display.clearDisplay();

  // WiFi Init (from secrets.h)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");

  // Time Init
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

// -------- GET TIMESTAMP --------
String getTimestamp() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00Z";
  }

  char timeString[30];
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(timeString);
}

void loop() {

  reedState = digitalRead(REED_PIN);

  // -------- DEBOUNCE LOGIC --------
  if (reedState != lastReedState && millis() - lastTrigger > 300) {

    lastTrigger = millis();

    String eventType;

    // ⚠️ Confirm logic based on your wiring
    if (reedState == HIGH) {
      openCount++;
      eventType = "OPEN";
    } else {
      closeCount++;
      eventType = "CLOSE";
    }

    String timestamp = getTimestamp();

    Serial.println(eventType + " @ " + timestamp);

    sendToFirestore(eventType, timestamp);

    lastReedState = reedState;
  }

  // -------- DISPLAY --------
  if (millis() - lastDisplayUpdate > 1000) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}

// -------- OLED DISPLAY --------
void updateDisplay() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 5);
  display.print("Door Monitor");

  display.setCursor(0, 20);
  display.print("Status: ");
  display.print(reedState == HIGH ? "OPEN" : "CLOSED");

  display.setCursor(0, 35);
  display.print("Open: ");
  display.print(openCount);

  display.setCursor(0, 50);
  display.print("Close: ");
  display.print(closeCount);

  display.display();
}

// -------- FIRESTORE --------
void sendToFirestore(String eventType, String timestamp) {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    String url = "https://firestore.googleapis.com/v1/projects/" + projectId +
                 "/databases/(default)/documents/events?key=" + String(API_KEY);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{ \"fields\": {"
                     "\"event\": {\"stringValue\": \"" + eventType + "\"},"
                     "\"openCount\": {\"integerValue\": \"" + String(openCount) + "\"},"
                     "\"closeCount\": {\"integerValue\": \"" + String(closeCount) + "\"},"
                     "\"timestamp\": {\"timestampValue\": \"" + timestamp + "\"},"
                     "\"device\": {\"stringValue\": \"toilet_1\"}"
                     "}}";

    int httpResponseCode = http.POST(payload);

    Serial.print("Response: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}