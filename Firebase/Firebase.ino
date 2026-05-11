#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"
#include "secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define REED_PIN 4
#define RESET_PIN 0

WebServer server(80);
Preferences prefs;
bool configMode = false;
unsigned long configTimeout = 0;
bool isFirstSetup = false;

int reedState = HIGH;
int lastReedState = HIGH;
int openCount = 0;
int closeCount = 0;

String projectId = "esp32-demo-project-47c9e";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

unsigned long lastDisplayUpdate = 0;
unsigned long lastTrigger = 0;
String savedSSID = "";
String savedPass = "";
String configPassword = "";

void saveCredentials(String ssid, String pass) {
  prefs.begin("airloo", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

bool loadCredentials() {
  prefs.begin("airloo", true);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();
  return savedSSID.length() > 0;
}

void clearCredentials() {
  prefs.begin("airloo", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
  savedSSID = "";
  savedPass = "";
}

void saveConfigPassword(String pwd) {
  prefs.begin("airloo", false);
  prefs.putString("admin_pwd", pwd);
  prefs.end();
}

bool loadConfigPassword() {
  prefs.begin("airloo", true);
  configPassword = prefs.getString("admin_pwd", "");
  prefs.end();
  return configPassword.length() > 0;
}

void clearConfigPassword() {
  prefs.begin("airloo", false);
  prefs.remove("admin_pwd");
  prefs.end();
  configPassword = "";
}

String loginPage() {
  return "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>AirLoo — Admin Login</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,sans-serif;background:#0B1E36;color:#E8F4FD;padding:20px;min-height:100vh;display:flex;flex-direction:column;align-items:center}"
    ".card{background:#162F52;border:1px solid #1E3E68;border-radius:16px;padding:28px;width:100%;max-width:400px;margin-top:60px}"
    ".logo{font-size:24px;font-weight:800;margin-bottom:4px}"
    ".logo span{color:#00C9A7}"
    ".sub{font-size:13px;color:#7A9EC5;margin-bottom:24px}"
    "label{display:block;font-size:13px;font-weight:600;margin-bottom:6px;color:#E8F4FD}"
    "input{width:100%;padding:12px;border-radius:10px;border:1px solid #1E3E68;background:#0B1E36;color:#E8F4FD;font-size:15px;margin-bottom:16px;outline:none}"
    "input:focus{border-color:#00C9A7}"
    "button{width:100%;padding:14px;border:none;border-radius:10px;background:#00C9A7;color:#0B1E36;font-size:16px;font-weight:700;cursor:pointer}"
    "button:hover{background:#00deb8}"
    "#err{color:#FF5C8A;font-size:13px;margin-top:10px;text-align:center}"
    "</style></head><body>"
    "<div class='card'>"
    "<div class='logo'>Air<span>Loo</span></div>"
    "<div class='sub'>Enter admin password to change WiFi</div>"
    "<form id='f'>"
    "<input type='password' name='pwd' id='pwd' placeholder='Admin password' required>"
    "<button type='submit'>Unlock</button>"
    "</form>"
    "<div id='err'></div>"
    "</div>"
    "<script>document.getElementById('f').onsubmit=async function(e){"
    "e.preventDefault();var r=await fetch('/unlock?pwd='+encodeURIComponent(document.getElementById('pwd').value));"
    "var t=await r.text();if(t=='ok'){location.href='/config'}else{document.getElementById('err').textContent=t}}"
    "</script></body></html>";
}

bool checkAdminAuth() {
  if (!server.hasArg("pwd")) return false;
  String pwd = server.arg("pwd");
  return pwd == configPassword;
}

void handleUnlock() {
  if (checkAdminAuth()) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(200, "text/plain", "Wrong password");
  }
}

bool connectToWiFi(String ssid, String pass, int timeoutMs = 10000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting to ");
  Serial.print(ssid);

  int waited = 0;
  while (WiFi.status() != WL_CONNECTED && waited < timeoutMs) {
    delay(200);
    Serial.print(".");
    waited += 200;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 5);
    display.print("Connecting...");
    display.setCursor(0, 25);
    display.print(ssid.substring(0, 18));
    for (int i = 0; i < (waited / 400) % 4; i++) display.print(".");
    display.display();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    return true;
  }
  Serial.println("\nFailed!");
  return false;
}

String scanNetworksHTML() {
  int n = WiFi.scanComplete();
  String html = "";
  if (n == WIFI_SCAN_FAILED) {
    html = "<option value=''>Scan failed</option>";
  } else if (n == 0) {
    html = "<option value=''>No networks found</option>";
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String bars = (rssi > -50) ? "▂▄▆█" : (rssi > -65) ? "▂▄▆" : (rssi > -80) ? "▂▄" : "▂";
      if (ssid.length() == 0) continue;
      html += "<option value='" + ssid + "'>" + ssid + "  " + bars + "</option>";
    }
  }
  return html;
}

String configPage() {
  String setupField = isFirstSetup ? ""
    "<label for='admin_pwd'>Set Admin Password</label>"
    "<input type='password' name='admin_pwd' id='admin_pwd' placeholder='Used to protect config portal' required>"
    "<label for='admin_pwd2'>Confirm Password</label>"
    "<input type='password' name='admin_pwd2' id='admin_pwd2' placeholder='Re-enter password' required>"
    "" : "";

  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>AirLoo WiFi Setup</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,sans-serif;background:#0B1E36;color:#E8F4FD;padding:20px;min-height:100vh;display:flex;flex-direction:column;align-items:center}"
    ".card{background:#162F52;border:1px solid #1E3E68;border-radius:16px;padding:28px;width:100%;max-width:420px;margin-top:40px}"
    "h1{font-size:22px;font-weight:700;margin-bottom:6px;color:#00C9A7}"
    ".sub{font-size:13px;color:#7A9EC5;margin-bottom:20px}"
    "label{display:block;font-size:13px;font-weight:600;margin-bottom:6px;color:#E8F4FD}"
    "select,input{width:100%;padding:12px;border-radius:10px;border:1px solid #1E3E68;background:#0B1E36;color:#E8F4FD;font-size:15px;margin-bottom:16px;outline:none}"
    "select:focus,input:focus{border-color:#00C9A7}"
    "button{width:100%;padding:14px;border:none;border-radius:10px;background:#00C9A7;color:#0B1E36;font-size:16px;font-weight:700;cursor:pointer}"
    "button:hover{background:#00deb8}"
    "#status{margin-top:16px;font-size:13px;text-align:center;color:#7A9EC5}"
    ".loader{display:none;width:20px;height:20px;border:2px solid #1E3E68;border-top:2px solid #00C9A7;border-radius:50%;animation:spin 0.8s linear infinite;margin:12px auto}"
    "@keyframes spin{to{transform:rotate(360deg)}}"
    ".logo{font-size:24px;font-weight:800;color:#E8F4FD;margin-bottom:4px}"
    ".logo span{color:#00C9A7}"
    ".refresh{display:block;text-align:right;font-size:12px;color:#4FC3F7;cursor:pointer;margin-top:-12px;margin-bottom:12px;text-decoration:none}"
    ".refresh:hover{text-decoration:underline}"
    "</style></head><body>"
    "<div class='card'>"
    "<div class='logo'>Air<span>Loo</span></div>"
    "<div class='sub'>Configure WiFi for your ESP32</div>"
    "<form id='form'>"
    "<label for='ssid'>Select WiFi Network</label>"
    "<select name='ssid' id='ssid' required>"
      + scanNetworksHTML() +
    "</select>"
    "<a href='#' class='refresh' onclick='rescan()'>⟳ Rescan</a>"
    "<label for='pass'>WiFi Password</label>"
    "<input type='password' name='pass' id='pass' placeholder='Enter WiFi password'>"
    + setupField +
    "<button type='submit'>Connect</button>"
    "</form>"
    "<div class='loader' id='loader'></div>"
    "<div id='status'></div>"
    "</div>"
    "<script>"
    "document.getElementById('form').onsubmit=async function(e){"
    "e.preventDefault();"
    "var p=document.getElementById('admin_pwd');"
    "var p2=document.getElementById('admin_pwd2');"
    "if(p&&p.value!=p2.value){document.getElementById('status').textContent='Passwords do not match!';return;}"
    "if(p&&p.value.length<4){document.getElementById('status').textContent='Password must be at least 4 characters!';return;}"
    "document.getElementById('loader').style.display='block';"
    "document.getElementById('status').textContent='Connecting...';"
    "var f=new FormData(this);"
    "var r=await fetch('/save',{method:'POST',body:new URLSearchParams(f)});"
    "var t=await r.text();"
    "document.getElementById('loader').style.display='none';"
    "document.getElementById('status').innerHTML=t;"
    "}"
    "async function rescan(){"
    "document.getElementById('status').textContent='Scanning...';"
    "var r=await fetch('/scan');"
    "var h=await r.text();"
    "document.getElementById('ssid').innerHTML=h;"
    "document.getElementById('status').textContent='Scan complete';"
    "}"
    "</script></body></html>";
  return page;
}

void handleRoot() {
  WiFi.scanDelete();
  WiFi.scanNetworks();
  if (isFirstSetup) {
    server.send(200, "text/html", configPage());
  } else {
    server.send(200, "text/html", loginPage());
  }
}

void handleConfig() {
  if (isFirstSetup) {
    WiFi.scanDelete();
    WiFi.scanNetworks();
    server.send(200, "text/html", configPage());
    return;
  }
  if (!checkAdminAuth()) {
    server.send(200, "text/html", loginPage());
    return;
  }
  WiFi.scanDelete();
  WiFi.scanNetworks();
  server.send(200, "text/html", configPage());
}

void handleScan() {
  WiFi.scanDelete();
  WiFi.scanNetworks();
  server.send(200, "text/html", scanNetworksHTML());
}

void handleSave() {
  if (!server.hasArg("ssid")) {
    server.send(200, "text/plain", "SSID is required");
    return;
  }

  if (isFirstSetup) {
    if (!server.hasArg("admin_pwd") || !server.hasArg("admin_pwd2")) {
      server.send(200, "text/plain", "Admin password is required");
      return;
    }
    String pwd = server.arg("admin_pwd");
    String pwd2 = server.arg("admin_pwd2");
    if (pwd != pwd2) {
      server.send(200, "text/plain", "Passwords do not match!");
      return;
    }
    if (pwd.length() < 4) {
      server.send(200, "text/plain", "Password must be at least 4 characters");
      return;
    }
    saveConfigPassword(pwd);
  }

  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  pass.trim();

  server.send(200, "text/plain",
    "Credentials saved! Rebooting...<br>"
    "SSID: " + ssid + "<br>"
    "Once connected, open http://airloo.local or check your router."
  );

  delay(100);
  saveCredentials(ssid, pass);
  delay(500);
  ESP.restart();
}

void startConfigPortal() {
  configMode = true;
  configTimeout = millis() + 300000;

  WiFi.mode(WIFI_AP);
  WiFi.softAP("AirLoo-Config");
  IPAddress apIP = WiFi.softAPIP();

  Serial.println("\n===================================");
  Serial.println("CONFIG MODE");
  Serial.print("Connect to AP: AirLoo-Config");
  Serial.print("\nOpen browser to: http://");
  Serial.println(apIP);
  Serial.println("===================================");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  display.print("Config Mode");
  display.setCursor(0, 22);
  display.print("Connect to:");
  display.setCursor(0, 33);
  display.print("\"AirLoo-Config\"");
  display.setCursor(0, 50);
  display.print("Open ");
  display.print(apIP);
  display.display();

  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/unlock", handleUnlock);
  server.on("/scan", handleScan);
  server.on("/save", handleSave);
  server.begin();

  while (configMode) {
    server.handleClient();
    if (configTimeout > 0 && millis() > configTimeout) {
      Serial.println("Config mode timeout. Rebooting...");
      delay(100);
      ESP.restart();
    }
    delay(10);
  }
}

void checkResetButton() {
  pinMode(RESET_PIN, INPUT_PULLUP);
  delay(50);
  if (digitalRead(RESET_PIN) == LOW) {
    Serial.println("BOOT held on power-up — full factory reset!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 5);
    display.print("Factory Reset...");
    display.display();
    clearCredentials();
    clearConfigPassword();
    delay(1000);
    ESP.restart();
  }
}

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00Z";
  }
  char timeString[30];
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(timeString);
}

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

void setup() {
  Serial.begin(115200);
  pinMode(REED_PIN, INPUT_PULLUP);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  display.print("AirLoo v2");
  display.setCursor(0, 25);
  display.print("Booting...");
  display.display();

  checkResetButton();

  bool hasPwd = loadConfigPassword();
  isFirstSetup = !hasPwd;

  if (loadCredentials()) {
    if (connectToWiFi(savedSSID, savedPass, 15000)) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    } else {
      startConfigPortal();
    }
  } else {
    startConfigPortal();
  }
}

void loop() {
  static unsigned long btnPressStart = 0;
  if (digitalRead(RESET_PIN) == LOW) {
    if (btnPressStart == 0) btnPressStart = millis();
    else if (millis() - btnPressStart > 5000) {
      Serial.println("BOOT held 5s — restarting into config portal...");
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 5);
      display.print("Config Portal...");
      display.display();
      startConfigPortal();
    }
  } else {
    btnPressStart = 0;
  }

  reedState = digitalRead(REED_PIN);

  if (reedState != lastReedState && millis() - lastTrigger > 300) {
    lastTrigger = millis();
    String eventType;
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

  if (millis() - lastDisplayUpdate > 1000) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}
