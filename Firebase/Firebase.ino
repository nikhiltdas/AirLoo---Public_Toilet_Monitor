#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"
#include "secrets.h"
#include "esp_sleep.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define REED_PIN 4
#define RESET_PIN 0
#define uS_TO_S_FACTOR 1000000ULL

WebServer server(80);
Preferences prefs;
bool configMode = false, isFirstSetup = false;
unsigned long configTimeout;
int openCount = 0, closeCount = 0;
String pjId = "esp32-demo-project-47c9e";
const char* ntpSrv = "pool.ntp.org";
const long gmtOff = 0, dayOff = 0;

static const char HDR[] = "<!DOCTYPE html><html><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>AirLoo</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,sans-serif;background:#0B1E36;color:#E8F4FD;padding:20px;min-height:100vh;display:flex;flex-direction:column;align-items:center}.card{background:#162F52;border:1px solid #1E3E68;border-radius:16px;padding:28px;width:100%;max-width:420px;margin-top:40px}.logo{font-size:24px;font-weight:800;margin-bottom:4px}.logo span{color:#00C9A7}.sub{font-size:13px;color:#7A9EC5;margin-bottom:20px}label{display:block;font-size:13px;font-weight:600}select,input{width:100%;padding:12px;border-radius:10px;border:1px solid #1E3E68;background:#0B1E36;color:#E8F4FD;font-size:15px;margin-bottom:16px;outline:none}select:focus,input:focus{border-color:#00C9A7}button{width:100%;padding:14px;border:none;border-radius:10px;background:#00C9A7;color:#0B1E36;font-size:16px;font-weight:700;cursor:pointer}button:hover{background:#00deb8}#s{margin-top:16px;font-size:13px;text-align:center;color:#7A9EC5}.ld{display:none;width:20px;height:20px;border:2px solid #1E3E68;border-top:2px solid #00C9A7;border-radius:50%;animation:spin .8s linear infinite;margin:12px auto}@keyframes spin{to{transform:rotate(360deg)}}</style></head><body><div class='card'><div class='logo'>Air<span>Loo</span></div>";

static const char LOGIN_TAIL[] = "<div class='sub'>Enter admin password</div><form id=f><input type=password name=pwd id=pwd placeholder='Admin password' required><button type=submit>Unlock</button></form><div id=e></div></div><script>document.getElementById('f').onsubmit=async function(e){e.preventDefault();var r=await fetch('/unlock?pwd='+encodeURIComponent(document.getElementById('pwd').value));var t=await r.text();if(t=='ok'){location.href='/config'}else{document.getElementById('e').textContent=t}}</script></body></html>";

static const char CONF_HDR[] = "<div class='sub'>Configure WiFi</div><form id=f><label>WiFi Network</label><select name=ssid id=ssid required>";
static const char CONF_MID[] = "</select><a href='#' onclick='rescan()' style='display:block;text-align:right;font-size:12px;color:#4FC3F7;cursor:pointer;margin-top:-12px;margin-bottom:12px'>⟳ Rescan</a><label>WiFi Password</label><input type=password name=pass id=pass placeholder='Enter WiFi password'>";
static const char CONF_TAIL[] = "<button type=submit>Connect</button></form><div class=ld id=ld></div><div id=s></div></div><script>document.getElementById('f').onsubmit=async function(e){e.preventDefault();var p=document.getElementById('ap');var p2=document.getElementById('ap2');if(p&&p.value!=p2.value){document.getElementById('s').textContent='Passwords do not match!';return}if(p&&p.value.length<4){document.getElementById('s').textContent='Password too short!';return}document.getElementById('ld').style.display='block';document.getElementById('s').textContent='Connecting...';var f=new FormData(this);var r=await fetch('/save',{method:'POST',body:new URLSearchParams(f)});var t=await r.text();document.getElementById('ld').style.display='none';document.getElementById('s').innerHTML=t};async function rescan(){document.getElementById('s').textContent='Scanning...';var r=await fetch('/scan');document.getElementById('ssid').innerHTML=await r.text();document.getElementById('s').textContent='Scan complete'}</script></body></html>";

static const char SETUP_FLD[] = "<label>Set Admin Password</label><input type=password name=admin_pwd id=ap placeholder='Min 4 characters' required><label>Confirm Password</label><input type=password name=admin_pwd2 id=ap2 placeholder='Re-enter password' required>";

String savedSSID, savedPass, cfgPwd;
int lastReedState = -1;
bool credsLoaded = false;

void svCred(String s, String p) { prefs.begin("airloo",0); prefs.putString("ssid",s); prefs.putString("pass",p); prefs.end(); }
bool ldCred() { prefs.begin("airloo",1); savedSSID=prefs.getString("ssid",""); savedPass=prefs.getString("pass",""); prefs.end(); return savedSSID.length()>0; }
void clrCred() { prefs.begin("airloo",0); prefs.remove("ssid"); prefs.remove("pass"); prefs.end(); savedSSID=""; savedPass=""; }
void svPwd(String p) { prefs.begin("airloo",0); prefs.putString("pwd",p); prefs.end(); }
bool ldPwd() { prefs.begin("airloo",1); cfgPwd=prefs.getString("pwd",""); prefs.end(); return cfgPwd.length()>0; }
void clrPwd() { prefs.begin("airloo",0); prefs.remove("pwd"); prefs.end(); cfgPwd=""; }

bool cnWiFi(String s, String p, int to=15000) {
  WiFi.mode(WIFI_STA); WiFi.setSleep(true); WiFi.begin(s.c_str(), p.c_str());
  int w=0; while (WiFi.status()!=WL_CONNECTED && w<to) { delay(100); w+=100; }
  return WiFi.status()==WL_CONNECTED;
}

int scanOpts(char* buf, int sz) {
  int n=WiFi.scanComplete(), pos=0;
  if (n<=0) pos=snprintf(buf,sz,"<option>No networks</option>");
  else for (int i=0;i<n;i++) {
    String s=WiFi.SSID(i); if (!s.length()) continue;
    const char* b=(WiFi.RSSI(i)>-50)?"▂▄▆█":(WiFi.RSSI(i)>-65)?"▂▄▆":(WiFi.RSSI(i)>-80)?"▂▄":"▂";
    pos+=snprintf(buf+pos,sz-pos,"<option value='%s'>%s %s</option>",s.c_str(),s.c_str(),b);
    if (pos>=sz) break;
  }
  return pos;
}

void sendPage(const char* hdr, const char* mid, const char* tail, bool dyn=false) {
  char buf[2300]; int p=0;
  p+=snprintf(buf+p,sizeof(buf)-p,"%s%s",HDR,hdr);
  if (dyn) p+=scanOpts(buf+p,sizeof(buf)-p);
  p+=snprintf(buf+p,sizeof(buf)-p,"%s%s%s",CONF_MID,mid?mid:"",tail?tail:CONF_TAIL);
  server.send(200,"text/html",buf);
}

void hRoot() {
  WiFi.scanDelete(); WiFi.scanNetworks();
  if (isFirstSetup) { sendPage(CONF_HDR,"",SETUP_FLD,1); return; }
  char b[1200]; snprintf(b,sizeof(b),"%s%s",HDR,LOGIN_TAIL); server.send(200,"text/html",b);
}
void hConfig() {
  WiFi.scanDelete(); WiFi.scanNetworks();
  if (isFirstSetup) { sendPage(CONF_HDR,"",SETUP_FLD,1); return; }
  if (!server.hasArg("pwd")||server.arg("pwd")!=cfgPwd) { char b[1200]; snprintf(b,sizeof(b),"%s%s",HDR,LOGIN_TAIL); server.send(200,"text/html",b); return; }
  sendPage(CONF_HDR,"","",1);
}
void hScan() { WiFi.scanDelete(); WiFi.scanNetworks(); char b[1800]; scanOpts(b,sizeof(b)); server.send(200,"text/html",b); }
void hUnlock() { server.send(200,"text/plain",server.hasArg("pwd")&&server.arg("pwd")==cfgPwd?"ok":"Wrong password"); }

void hSave() {
  if (!server.hasArg("ssid")) { server.send(200,"text/plain","SSID required"); return; }
  if (isFirstSetup) {
    if (!server.hasArg("admin_pwd")||!server.hasArg("admin_pwd2")) { server.send(200,"text/plain","Password required"); return; }
    String p=server.arg("admin_pwd"),p2=server.arg("admin_pwd2");
    if (p!=p2||p.length()<4) { server.send(200,"text/plain",p!=p2?"Passwords don't match!":"Password min 4 chars"); return; }
    svPwd(p);
  }
  String s=server.arg("ssid"),p=server.arg("pass"); s.trim(); p.trim();
  server.send(200,"text/plain","Saved! Rebooting..."); delay(100); svCred(s,p); delay(500); ESP.restart();
}

void startCP() {
  configMode=1; configTimeout=millis()+300000;
  WiFi.mode(WIFI_AP); WiFi.softAP("AirLoo-Config");
  Serial.printf("\nCONFIG: http://%s\n",WiFi.softAPIP().toString().c_str());
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(0,5); display.print("Config Mode");
  display.setCursor(0,22); display.print("Connect to:");
  display.setCursor(0,33); display.print("\"AirLoo-Config\"");
  display.setCursor(0,50); display.print("Open "); display.print(WiFi.softAPIP()); display.display();
  server.on("/",hRoot); server.on("/config",hConfig); server.on("/unlock",hUnlock); server.on("/scan",hScan); server.on("/save",hSave);
  server.begin();
  while (configMode) { server.handleClient(); if (millis()>configTimeout) ESP.restart(); delay(10); }
}

void showOLED(String l1, String l2="", String l3="", String l4="") {
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(0,5); display.print(l1);
  if (l2.length()) { display.setCursor(0,22); display.print(l2); }
  if (l3.length()) { display.setCursor(0,39); display.print(l3); }
  if (l4.length()) { display.setCursor(0,50); display.print(l4); }
  display.display();
}

void powerOff() {
  display.ssd1306_command(0xAE);
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
}

void sendFS(String ev, int oc, int cc, String ts) {
  if (WiFi.status()!=WL_CONNECTED) return;
  HTTPClient h; String url="https://firestore.googleapis.com/v1/projects/"+pjId+"/databases/(default)/documents/events?key="+String(API_KEY);
  h.begin(url); h.addHeader("Content-Type","application/json");
  char pl[400]; snprintf(pl,sizeof(pl),"{\"fields\":{\"event\":{\"stringValue\":\"%s\"},\"openCount\":{\"integerValue\":\"%d\"},\"closeCount\":{\"integerValue\":\"%d\"},\"timestamp\":{\"timestampValue\":\"%s\"},\"device\":{\"stringValue\":\"toilet_1\"}}}",ev.c_str(),oc,cc,ts.c_str());
  int r=h.POST(pl); Serial.printf("FS:%d\n",r); h.end();
}

String getTS() {
  struct tm t; if (!getLocalTime(&t)) return "1970-01-01T00:00:00Z";
  char b[30]; strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%SZ",&t); return String(b);
}

void goDeepSleep() {
  int level = digitalRead(REED_PIN)==HIGH ? 0 : 1;
  powerOff();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, level);
  Serial.printf("Sleep (wake on %s)\n",level?"LOW":"HIGH");
  esp_deep_sleep_start();
}

void handleWake(bool sendEvent=true) {
  delay(50);
  int state = digitalRead(REED_PIN);
  bool isOpen = state==HIGH;
  Serial.printf("Wake: %s (send=%d)\n",isOpen?"OPEN":"CLOSED",sendEvent);

  if (sendEvent) {
    if (isOpen) openCount++; else closeCount++;
    saveCounts();
    if (cnWiFi(savedSSID,savedPass,8000)) {
      configTime(gmtOff,dayOff,ntpSrv); delay(500);
      sendFS(isOpen?"OPEN":"CLOSE",openCount,closeCount,getTS());
    }
  }

  showOLED(isOpen?"DOOR OPEN":"DOOR CLOSED","Open:"+String(openCount),"Close:"+String(closeCount));
  delay(3000);
  goDeepSleep();
}

void chkRst() {
  pinMode(RESET_PIN,INPUT_PULLUP); delay(50);
  if (digitalRead(RESET_PIN)==LOW) { Serial.println("Factory reset"); clrCred(); clrPwd(); ESP.restart(); }
}

void readCounts() {
  prefs.begin("airloo",1);
  openCount = prefs.getInt("oc",0);
  closeCount = prefs.getInt("cc",0);
  prefs.end();
}

void saveCounts() {
  prefs.begin("airloo",0);
  prefs.putInt("oc",openCount);
  prefs.putInt("cc",closeCount);
  prefs.end();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21,22);
  pinMode(REED_PIN,INPUT_PULLUP);
  setCpuFrequencyMhz(80);

  bool fromDeepSleep = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;

  if (!display.begin(SSD1306_SWITCHCAPVCC,0x3C)) {
    Serial.println("OLED fail");
    if (!fromDeepSleep) { while(1); }
  }

  if (fromDeepSleep) {
    readCounts();
    handleWake(true);
    return;
  }

  showOLED("AirLoo v2","Booting...");
  chkRst();
  credsLoaded = ldCred();
  isFirstSetup = !ldPwd();

  if (!credsLoaded || isFirstSetup) {
    startCP();
  }

  readCounts();
  handleWake(false);
}

void loop() {}
