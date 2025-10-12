#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include <WebServer.h>
#include <vector>
#include <string.h> 

// ================== Firmware ==================
static const char* FW_VERSION = "1.5.0";

// ================== Hardware ==================
#define OLED_W     128
#define OLED_H      64
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

// RC522 (VSPI)
#define RC522_SS   5
#define RC522_RST  27
MFRC522 rfid(RC522_SS, RC522_RST);

// Servo gate
#define SERVO_PIN         25
#define GATE_CLOSED_DEG    0
#define GATE_OPEN_DEG     90
#define GATE_OPEN_MS   2000UL
Servo gateServo;
unsigned long gateCloseAtMs = 0;
bool gateIsOpen = false;

#define LED_R_PIN 26
#define LED_G_PIN 33
#define LED_B_PIN 32
inline void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R_PIN, r ? HIGH : LOW);
  digitalWrite(LED_G_PIN, g ? HIGH : LOW);
  digitalWrite(LED_B_PIN, b ? HIGH : LOW);
}
inline void ledIdleBlue()       { setLED(false, false, true); }
inline void ledAccessGranted()  { setLED(false, true,  false); }
inline void ledAccessDenied()   { setLED(true,  false, false); }

// ================== Network / Time ==================
const char* WIFI_SSID = "Hanu";
const char* WIFI_PASS = "12345678";

// Admin dashboard alert endpoint (HTTP POST JSON)
const char* ADMIN_ALERT_URL = "http://localhost:3000/api/alerts";

// Timezone: Vietnam (UTC+7). No DST.
const long  GMT_OFFSET_SEC = 7 * 3600;
const int   DST_OFFSET_SEC = 0;

// ================== Credits & Users ==================
static const long COST_PER_EXIT   = 3000;      // 3,000đ per exit
static const long DEFAULT_CREDIT  = 100000;    // default for new DYN

enum UserType : uint8_t { U_STA=0, U_DYN=1 };

struct User {
  String uid;     
  String name;
  long   credit;  // VND
  bool   in;      // false OUT, true IN
  UserType type;
};


User staticUsers[] = {
  // {"04:A1:B2:C3", "Nguyen Van A", 100000, false, U_STA},
};
constexpr size_t STATIC_COUNT = sizeof(staticUsers)/sizeof(staticUsers[0]);
std::vector<User> dynamicUsers;

// Preferences / NVS
Preferences prefs;
static const char* NVS_NS  = "rfid-gate";
static const char* NVS_DYN = "dyn1"; // blob key for dynamic users

// ================== Input Mode / Events ==================
struct { bool active=false; } InputMode;

// Last scanned card tracking for input mode
struct {
  String uid = "";
  uint32_t timestamp = 0;
  bool isNew = false;
} LastScan;

struct Ev { uint32_t t; String msg; };
std::vector<Ev> events;
void logEvent(const String& m){
  events.push_back({(uint32_t)(millis()/1000), m});
  if (events.size() > 200) events.erase(events.begin()); // cap
}

// ================== UI State ==================
bool showing = false;
unsigned long showUntilMs = 0;

// ================== HTML (Simple Device Info) ==================
const char INDEX_HTML_MIN[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>ESP32 Gate System</title>
<style>
body{margin:12px;background:#111;color:#eee;font:14px/1.4 system-ui,Segoe UI,Arial}
small{color:#9aa}h1{font-size:18px;margin:0 0 8px}
.card{background:#1a1a1a;border:1px solid #2b2b2b;border-radius:8px;padding:10px;margin:10px 0}
button{padding:6px 9px;margin:2px;border:1px solid #3a3a3a;border-radius:6px;background:#242424;color:#fff;cursor:pointer}
button:hover{filter:brightness(1.1)}
.pill{display:inline-block;border:1px solid #3a3a3a;border-radius:999px;padding:2px 8px}
.ok{color:#5f5}.warn{color:#ff5}.bad{color:#f77}.mono{font-family:ui-monospace,Consolas,monospace}
</style></head><body>
<h1>ESP32 Gate System <small id=fw>FW…</small></h1>
<div class=card><b>Device Status</b>
<div id=info class=mono>Loading...</div>
<div>Input Mode: <span id=input class=pill>?</span></div>
<div>Gate: <span id=gate class=pill>?</span></div>
</div>
<div class=card><b>Quick Controls</b><br>
<button onclick="P('/api/open')">Open Gate</button>
<button onclick="LED('GREEN')">LED Green</button>
<button onclick="LED('BLUE')">LED Blue</button>
<button onclick="LED('RED')">LED Red</button>
<button onclick="LED('OFF')">LED Off</button>
</div>
<div class=card><b>Users: <span id=userCount>0</span></b>
<div>Static: <span id=staticCount>0</span> | Dynamic: <span id=dynamicCount>0</span></div>
</div>
<div class=card><b>Recent Events</b>
<pre id=ev class=mono style="white-space:pre-wrap;margin:6px 0 0;max-height:200px;overflow-y:auto">Loading...</pre>
</div>
<script>
const $=id=>document.getElementById(id);
const G=u=>fetch(u).then(r=>{if(!r.ok)throw 0;const c=r.headers.get('Content-Type')||'';return c.includes('json')?r.json():r.text()});
const postForm=(u,obj)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:Object.entries(obj).map(([k,v])=>k+'='+encodeURIComponent(v)).join('&')});
const P=u=>postForm(u,{});
const LED=c=>postForm('/api/led',{c});
function pill(el,status,text){el.className='pill '+(status?'ok':'bad');el.textContent=text;}
async function R(){try{
  const i=await G('/api/info');
  $('fw').textContent='FW '+i.version;
  $('info').textContent=`IP: ${i.ip} | RSSI: ${i.rssi}dBm | Uptime: ${i.uptime_s}s | Free Heap: ${i.heap}`;
  $('userCount').textContent=i.users;
  $('staticCount').textContent=i.static;
  $('dynamicCount').textContent=i.dynamic;
  
  const st=await G('/api/state');
  pill($('input'), st.inputMode, st.inputMode?'ACTIVE':'INACTIVE');
  pill($('gate'), !st.gateOpen, st.gateOpen?'OPEN':'CLOSED');
  
  const ev=await G('/api/events');
  const recentEvents = (ev.events||[]).slice(-10);
  $('ev').textContent=recentEvents.map(e=>`[${e.t}s] ${e.msg}`).join('\n')||'No events';
}catch(e){
  $('info').textContent='Connection error';
}}
R();setInterval(R,3000);
</script></body></html>)HTML";


String uidToHex(const MFRC522::Uid& uid) {
  String s;
  for (byte i = 0; i < uid.size; i++) { if (uid.uidByte[i] < 0x10) s += "0"; s += String(uid.uidByte[i], HEX); if (i + 1 != uid.size) s += ":"; }
  s.toUpperCase(); return s;
}
int findStaticIndex(const String& uid)  { for (size_t i=0;i<STATIC_COUNT;++i) if (uid==staticUsers[i].uid) return (int)i; return -1; }
int findDynamicIndex(const String& uid) { for (size_t i=0;i<(int)dynamicUsers.size();++i) if (uid==dynamicUsers[i].uid) return (int)i; return -1; }
int findUserIndexCombined(const String& uid){
  int i=findStaticIndex(uid); if(i>=0) return i;
  int j=findDynamicIndex(uid); if(j>=0) return (int)(STATIC_COUNT+j);
  return -1;
}
const char* greetingForHour(int h){ if(h<12) return "Good Morning"; if(h<18) return "Good Afternoon"; return "Good Evening"; }
String vndFormat(long vnd){ String s=String(vnd),out; int n=s.length(); for(int i=0;i<n;++i){ out+=s[i]; int r=n-i-1; if(r>0 && r%3==0) out+=","; } out+="đ"; return out; }

// ================== OLED UI ==================
void drawHeaderWithClock(const char* title) {
  display.fillRect(0, 0, OLED_W, 22, SSD1306_BLACK);
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println(title);
  time_t now=time(nullptr); struct tm tm_info; localtime_r(&now,&tm_info);
  char buf[24]; snprintf(buf,sizeof(buf),"%04d-%02d-%02d %02d:%02d", tm_info.tm_year+1900, tm_info.tm_mon+1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min);
  display.setCursor(0, 10); display.println(buf);
  display.drawLine(0, 20, OLED_W, 20, SSD1306_WHITE);
}
void showIdle() {
  drawHeaderWithClock("Gate System");
  time_t now=time(nullptr); struct tm tm_info; localtime_r(&now,&tm_info);
  display.fillRect(0, 22, OLED_W, OLED_H-22, SSD1306_BLACK);
  display.setCursor(0, 28); display.print(greetingForHour(tm_info.tm_hour)); display.println(" ...");
  display.setCursor(0, 40); display.println("Please scan your ID");
  display.setCursor(0, 54); display.println("Status: Waiting...");
  display.display();
  ledIdleBlue();
}

// ================== WiFi / Time ==================
void connectWiFi() {
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi connecting to %s ...\n", WIFI_SSID);
  unsigned long t0=millis(); while(WiFi.status()!=WL_CONNECTED && millis()-t0<15000){ delay(250); Serial.print("."); }
  Serial.println(); if(WiFi.status()==WL_CONNECTED){ Serial.print("WiFi IP: "); Serial.println(WiFi.localIP()); } else { Serial.println("WiFi connect failed (offline)."); }
}
void setupTime() {
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
  for (int i=0;i<20;++i){ time_t now=time(nullptr); if(now>1700000000) break; delay(200); }
}

// ================== WEB SERVER ==================
WebServer server(80);

String normalizeUID(String in){
  in.toUpperCase(); in.replace("-", ":"); in.replace("_", ":");
  if (in.indexOf(':')<0 && (in.length()%2==0)) { String out; for(size_t i=0;i<in.length();i+=2){ out+=in.substring(i,i+2); if(i+2<in.length()) out+=":"; } in=out; }
  int last=0, parts=0;
  while(true){ int p=in.indexOf(':',last); String tok=(p<0)?in.substring(last):in.substring(last,p);
    if (tok.length()!=2) return String();
    for (int i=0;i<2;i++){ char c=tok[i]; if(!((c>='0'&&c<='9')||(c>='A'&&c<='F'))) return String(); }
    parts++; if(p<0) break; last=p+1;
  }
  if (parts<4) return String(); return in;
}
bool uidExistsExcept(const String& uid, int exceptIdxCombined){
  for(size_t i=0;i<STATIC_COUNT;i++){ if((int)i==exceptIdxCombined) continue; if(staticUsers[i].uid==uid) return true; }
  for(size_t j=0;j<dynamicUsers.size();j++){ int comb=STATIC_COUNT+(int)j; if(comb==exceptIdxCombined) continue; if(dynamicUsers[j].uid==uid) return true; }
  return false;
}

void sendIndex(){ server.sendHeader("Cache-Control","no-cache"); server.send_P(200,"text/html; charset=utf-8",INDEX_HTML_MIN,strlen_P(INDEX_HTML_MIN)); }
void sendJSON(const String& s){ server.sendHeader("Cache-Control","no-store"); server.send(200,"application/json; charset=utf-8",s); }
String jsonEscape(const String& in){ String o; o.reserve(in.length()+8); for(size_t i=0;i<in.length();i++){ char c=in[i]; if(c=='"'||c=='\\'){ o+='\\'; o+=c; } else if(c=='\n'){ o+="\\n"; } else o+=c; } return o; }

// ================== HTTP Helpers ==================
inline bool isJsonRequest(){
  if (!server.hasHeader("Content-Type")) return false;
  String ct = server.header("Content-Type"); ct.toLowerCase();
  return ct.indexOf("application/json") >= 0;
}

inline String getJsonBody(){
  return isJsonRequest() ? server.arg("plain") : String();
}

inline bool isWhitespace(char c){
  return c==' '||c=='\t'||c=='\n'||c=='\r';
}

String jsonValue(const String& body, const char* key){
  if (!body.length()) return String();
  String pattern = "\""; pattern += key; pattern += "\"";
  int keyPos = body.indexOf(pattern); if (keyPos < 0) return String();
  int colon = body.indexOf(":", keyPos + pattern.length()); if (colon < 0) return String();
  int pos = colon + 1;
  while (pos < (int)body.length() && isWhitespace(body[pos])) pos++;
  if (pos >= (int)body.length()) return String();
  if (body[pos] == '"'){
    int end = pos + 1;
    while (end < (int)body.length()){
      char c = body[end];
      if (c == '"' && body[end-1] != '\\') break;
      end++;
    }
    if (end >= (int)body.length()) return String();
    return body.substring(pos + 1, end);
  }
  int end = pos;
  while (end < (int)body.length()){
    char c = body[end];
    if (c==',' || c=='}' || c==']' || isWhitespace(c)) break;
    end++;
  }
  String out = body.substring(pos, end);
  out.trim();
  return out;
}

bool jsonValueBool(const String& body, const char* key, bool& out){
  String val = jsonValue(body, key);
  if (!val.length()) return false;
  val.toLowerCase();
  if (val=="true" || val=="1" || val=="on") { out = true; return true; }
  if (val=="false" || val=="0" || val=="off") { out = false; return true; }
  return false;
}

bool jsonValueLong(const String& body, const char* key, long& out){
  String val = jsonValue(body, key);
  if (!val.length()) return false;
  out = val.toInt();
  return true;
}

// ================== DYNAMIC NVS (v1 blob, debounced saves) ==================
bool saveDynamicUsers(){
  size_t total = 1 + 2; // ver + count
  for (auto& u : dynamicUsers){
    uint8_t uidLen  = (uint8_t)std::min((int)u.uid.length(), 255);
    uint8_t nameLen = (uint8_t)std::min((int)u.name.length(), 255);
    total += 1 + uidLen + 1 + nameLen + 4 + 1 + 1;
  }
  std::vector<uint8_t> buf; buf.resize(total);
  uint8_t* p = buf.data();
  *p++ = 1; // version
  uint16_t cnt = (uint16_t)std::min((int)dynamicUsers.size(), 65535);
  *p++ = (uint8_t)(cnt & 0xFF); *p++ = (uint8_t)(cnt >> 8);
  for (size_t i=0;i<cnt;i++){
    auto& u = dynamicUsers[i];
    uint8_t uidLen  = (uint8_t)std::min((int)u.uid.length(), 255);
    uint8_t nameLen = (uint8_t)std::min((int)u.name.length(), 255);
    *p++ = uidLen;  memcpy(p, u.uid.c_str(), uidLen);  p += uidLen;
    *p++ = nameLen; memcpy(p, u.name.c_str(), nameLen); p += nameLen;
    int32_t cred = (int32_t)u.credit;
    *p++ = (uint8_t)(cred & 0xFF); *p++ = (uint8_t)((cred>>8)&0xFF); *p++ = (uint8_t)((cred>>16)&0xFF); *p++ = (uint8_t)((cred>>24)&0xFF);
    *p++ = (uint8_t)(u.in ? 1 : 0);
    *p++ = (uint8_t)u.type; // should be U_DYN
  }
  size_t written = prefs.putBytes(NVS_DYN, buf.data(), buf.size());
  bool ok = (written == buf.size());
  if (!ok) Serial.println("NVS saveDynamicUsers FAILED");
  return ok;
}
void loadDynamicUsers(){
  size_t len = prefs.getBytesLength(NVS_DYN);
  if (!len) return;
  std::vector<uint8_t> buf; buf.resize(len);
  size_t got = prefs.getBytes(NVS_DYN, buf.data(), buf.size());
  if (got != len || len < 3) { Serial.println("NVS dyn blob invalid"); return; }
  const uint8_t* p = buf.data(); const uint8_t* e = buf.data()+buf.size();
  uint8_t ver = *p++; if (ver != 1) { Serial.println("NVS dyn ver mismatch"); return; }
  if (p+2>e) return;
  uint16_t cnt = (uint16_t)(p[0] | (p[1]<<8)); p+=2;
  dynamicUsers.clear(); dynamicUsers.reserve(cnt);
  for (uint16_t i=0;i<cnt;i++){
    if (p>=e) break;
    if (p+1>e) break; uint8_t uidLen=*p++; if (p+uidLen>e) break; String uid = String((const char*)p).substring(0, uidLen); p+=uidLen;
    if (p+1>e) break; uint8_t nameLen=*p++; if (p+nameLen>e) break; String name = String((const char*)p).substring(0, nameLen); p+=nameLen;
    if (p+4+1+1>e) break;
    int32_t cred = (int32_t)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24)); p+=4;
    bool in = (*p++ != 0);
    uint8_t type = *p++;
    uid = normalizeUID(uid);
    if (uid=="" || uidExistsExcept(uid, -1)) continue;
    User u; u.uid=uid; u.name=name; u.credit=cred; u.in=in; u.type=(type==U_STA?U_DYN:(UserType)type);
    if (u.type != U_DYN) u.type = U_DYN;
    dynamicUsers.push_back(u);
  }
  Serial.printf("NVS loaded %u dynamic users\n", (unsigned)dynamicUsers.size());
}

// Debounce machinery
static bool dynDirty=false;
static unsigned long dynDirtyAt=0;
static const unsigned long DYN_SAVE_DELAY_MS = 1200;
inline void markDynamicDirty(bool immediate=false){
  if (immediate) { saveDynamicUsers(); dynDirty=false; }
  else { dynDirty=true; dynDirtyAt=millis(); }
}

// -------- Handlers --------
void handleRoot(){ sendIndex(); }

void handleInfo(){
  String ip = (WiFi.status()==WL_CONNECTED)? WiFi.localIP().toString() : "0.0.0.0";
  int32_t rssi = (WiFi.status()==WL_CONNECTED)? WiFi.RSSI() : 0;
  String ssid = (WiFi.status()==WL_CONNECTED)? String(WIFI_SSID) : "N/A";
  String j = "{";
  j += "\"fwVersion\":\""+String(FW_VERSION)+"\",";
  j += "\"version\":\""+String(FW_VERSION)+"\",";
  j += "\"ip\":\""+ip+"\",";
  j += "\"ssid\":\""+ssid+"\",";
  j += "\"rssi\":"+String(rssi)+",";
  j += "\"uptime\":"+String(millis()/1000)+",";
  j += "\"uptime_s\":"+String(millis()/1000)+",";
  j += "\"freeHeap\":"+String(ESP.getFreeHeap())+",";
  j += "\"heap\":"+String(ESP.getFreeHeap())+",";
  j += "\"users\":"+String(STATIC_COUNT + dynamicUsers.size())+",";
  j += "\"static\":"+String(STATIC_COUNT)+",";
  j += "\"dynamic\":"+String(dynamicUsers.size());
  j += "}";
  sendJSON(j);
}

void handleState(){
  String j = "{";
  j += "\"inputMode\":"+(InputMode.active?String("true"):String("false"))+",";
  j += "\"gateOpen\":"+(gateIsOpen?String("true"):String("false"))+",";
  j += "\"users\":[";
  bool first=true;
  for (size_t i=0;i<STATIC_COUNT;i++){
    if (!first) j += ","; first=false;
    j += "{\"name\":\""+jsonEscape(staticUsers[i].name)+"\",\"uid\":\""+staticUsers[i].uid+"\",\"type\":\"STA\",\"in\":"+(staticUsers[i].in?String("true"):String("false"))+",\"credit\":"+String(staticUsers[i].credit)+"}";
  }
  for (size_t k=0;k<dynamicUsers.size();k++){
    if (!first) j += ","; first=false;
    auto& u = dynamicUsers[k];
    j += "{\"name\":\""+jsonEscape(u.name)+"\",\"uid\":\""+u.uid+"\",\"type\":\"DYN\",\"in\":"+(u.in?String("true"):String("false"))+",\"credit\":"+String(u.credit)+"}";
  }
  j += "]}";
  sendJSON(j);
}

void gateOpen(); // fwd
void handleOpen(){ gateOpen(); sendJSON("{}"); }

void handleLED(){
  bool r = false, g = false, b = false;
  if (isJsonRequest()){
    String body = getJsonBody();
    bool temp=false;
    if (jsonValueBool(body, "r", temp)) r = temp;
    if (jsonValueBool(body, "g", temp)) g = temp;
    if (jsonValueBool(body, "b", temp)) b = temp;
    if (jsonValueBool(body, "red", temp)) r = temp;
    if (jsonValueBool(body, "green", temp)) g = temp;
    if (jsonValueBool(body, "blue", temp)) b = temp;
  } else {
    String c = server.hasArg("c")? server.arg("c") : "";
    c.toUpperCase();
    if      (c=="GREEN") { r=false; g=true;  b=false; }
    else if (c=="BLUE")  { r=false; g=false; b=true;  }
    else if (c=="RED")   { r=true;  g=false; b=false; }
    else if (c=="OFF")   { r=false; g=false; b=false; }
  }
  setLED(r, g, b);
  sendJSON("{}");
}

void handleInputMode(){
  String mode = server.hasArg("mode") ? server.arg("mode") : String();
  mode.toLowerCase();
  if (mode == "on") {
    InputMode.active = true;
    logEvent("Input Mode ON");
    // Clear any previous scan data when activating
    LastScan.uid = "";
    LastScan.timestamp = 0;
    LastScan.isNew = false;
  } else if (mode == "off") {
    InputMode.active = false;
    logEvent("Input Mode OFF");
    // Clear scan data when deactivating
    LastScan.uid = "";
    LastScan.timestamp = 0;
    LastScan.isNew = false;
  } else if (isJsonRequest()) {
    String body = getJsonBody();
    bool val=false;
    if (jsonValueBool(body, "active", val)) {
      InputMode.active = val;
      logEvent(String("Input Mode ") + (val ? "ON" : "OFF"));
      if (!val) {
        // Clear scan data when deactivating
        LastScan.uid = "";
        LastScan.timestamp = 0;
        LastScan.isNew = false;
      }
    }
  }
  sendJSON("{\"active\":"+(InputMode.active?String("true"):String("false"))+"}");
}

void handleUsersAdd(){
  // Remove admin check - input mode handles user registration through admin panel
  String u = server.hasArg("uid") ? server.arg("uid") : String();
  String name = server.hasArg("name") ? server.arg("name") : String();
  long credit = server.hasArg("credit") ? server.arg("credit").toInt() : DEFAULT_CREDIT;
  
  if (isJsonRequest()) {
    String body = getJsonBody();
    if (u == "") u = jsonValue(body, "uid");
    if (name == "") name = jsonValue(body, "name");
    long parsedCredit = 0;
    if (jsonValueLong(body, "credit", parsedCredit)) credit = parsedCredit;
  }
  
  String uid = normalizeUID(u);
  if (uid=="" || uidExistsExcept(uid, -1)) { 
    server.send(400, "text/plain", "bad or duplicate uid"); 
    return; 
  }
  
  if (name == "") {
    name = String("User #") + String(dynamicUsers.size() + 1);
  }
  
  User nu; 
  nu.uid = uid; 
  nu.name = name;
  nu.credit = credit; 
  nu.in = false; 
  nu.type = U_DYN;
  
  dynamicUsers.push_back(nu);
  markDynamicDirty(true); // immediate save on enroll
  logEvent("Add user " + uid + " (" + name + ")");
  sendJSON("{\"success\":true,\"user\":{\"uid\":\""+uid+"\",\"name\":\""+jsonEscape(name)+"\",\"credit\":"+String(credit)+"}}");
}

void handleUsersDelete(){
  // Remove admin check - allow deletion through admin panel
  String uid = server.hasArg("uid") ? server.arg("uid") : String();
  
  if (isJsonRequest()) {
    String body = getJsonBody();
    if (uid == "") uid = jsonValue(body, "uid");
  }
  
  if (uid == "") { 
    server.send(400, "text/plain", "uid required"); 
    return; 
  }
  
  String normalizedUID = normalizeUID(uid);
  int idx = findUserIndexCombined(normalizedUID);
  
  if (idx < 0) { 
    server.send(404, "text/plain", "user not found"); 
    return; 
  }
  
  if (idx < (int)STATIC_COUNT) { 
    server.send(403, "text/plain", "cannot delete static user"); 
    return; 
  }
  
  int di = idx - (int)STATIC_COUNT;
  if (di < 0 || di >= (int)dynamicUsers.size()) { 
    server.send(404, "text/plain", "user not found"); 
    return; 
  }
  
  String deletedName = dynamicUsers[di].name;
  logEvent("Delete user " + normalizedUID + " (" + deletedName + ")");
  dynamicUsers.erase(dynamicUsers.begin() + di);
  markDynamicDirty(true); // immediate save on delete
  sendJSON("{\"success\":true,\"message\":\"User deleted\"}");
}

void handleUsersClear(){
  // Remove admin check - allow clearing through admin panel
  String what = server.hasArg("what") ? server.arg("what") : "dynamic";
  
  if (isJsonRequest()) {
    String body = getJsonBody();
    String jsonWhat = jsonValue(body, "what");
    if (jsonWhat.length() > 0) what = jsonWhat;
  }
  
  if (what == "dynamic") { 
    int count = dynamicUsers.size();
    dynamicUsers.clear(); 
    markDynamicDirty(true); 
    logEvent("Clear " + String(count) + " dynamic users"); 
    sendJSON("{\"success\":true,\"message\":\"" + String(count) + " users cleared\"}");
  } else {
    sendJSON("{\"success\":false,\"error\":\"Invalid clear type\"}");
  }
}

void handleCreditAdd(){
  int idx = -1;
  long v = 0;
  if (server.hasArg("idx")) idx = server.arg("idx").toInt();
  if (server.hasArg("v")) v = server.arg("v").toInt();

  if (isJsonRequest()) {
    String body = getJsonBody();
    String uid = jsonValue(body, "uid");
    if (uid.length()) idx = findUserIndexCombined(normalizeUID(uid));
    long parsed = 0;
    if (jsonValueLong(body, "amount", parsed)) v = parsed;
  }

  if (idx < 0) { server.send(400, "text/plain", "invalid uid"); return; }
  if (v == 0) { server.send(400, "text/plain", "amount"); return; }
  
  if (idx < (int)STATIC_COUNT) {
    staticUsers[idx].credit += v; // persist full static
    saveStaticState(idx);
    logEvent("Credit +"+String(v)+" to "+staticUsers[idx].uid);
  } else {
    int di = idx - (int)STATIC_COUNT;
    if (di<0 || di>=(int)dynamicUsers.size()) { server.send(404, "text/plain", "not found"); return; }
    dynamicUsers[di].credit += v;
    markDynamicDirty(true); 
    logEvent("Credit +"+String(v)+" to "+dynamicUsers[di].uid);
  }
  sendJSON("{}");
}

void handleStateSet(){
  int idx = -1;
  bool vin = false;

  if (server.hasArg("idx")) idx = server.arg("idx").toInt();
  if (server.hasArg("in")) vin = server.arg("in").toInt()!=0;

  if (isJsonRequest()){
    String body = getJsonBody();
    long parsedIdx = 0;
    if (jsonValueLong(body, "idx", parsedIdx)) idx = (int)parsedIdx;
    bool parsedBool;
    if (jsonValueBool(body, "in", parsedBool)) vin = parsedBool;
  }

  if (idx < 0) { server.send(400, "text/plain", "idx"); return; }
  if (idx < (int)STATIC_COUNT) {
    staticUsers[idx].in = vin;
    saveStaticState(idx);
    logEvent(String("Set IN=")+(vin?"1":"0")+" for "+staticUsers[idx].uid);
  } else {
    int di = idx - (int)STATIC_COUNT;
    if (di<0 || di>=(int)dynamicUsers.size()) { server.send(404, "text/plain", "not found"); return; }
    dynamicUsers[di].in = vin;
    markDynamicDirty(false); // non-financial -> debounced
    logEvent(String("Set IN=")+(vin?"1":"0")+" for "+dynamicUsers[di].uid);
  }
  sendJSON("{}");
}

// Update endpoint (admin edit)
void handleUsersUpdate(){
  // Remove admin check - allow updates through admin panel
  String uid = server.hasArg("uid") ? server.arg("uid") : String();
  String name = server.hasArg("name") ? server.arg("name") : String();
  String creditStr = server.hasArg("credit") ? server.arg("credit") : String();
  String inStr = server.hasArg("in") ? server.arg("in") : String();
  
  if (isJsonRequest()) {
    String body = getJsonBody();
    if (uid == "") uid = jsonValue(body, "uid");
    if (name == "") name = jsonValue(body, "name");
    if (creditStr == "") creditStr = jsonValue(body, "credit");
    if (inStr == "") inStr = jsonValue(body, "in");
  }
  
  if (uid == "") { 
    server.send(400, "text/plain", "uid required"); 
    return; 
  }
  
  String normalizedUID = normalizeUID(uid);
  int idx = findUserIndexCombined(normalizedUID);
  
  if (idx < 0) { 
    server.send(404, "text/plain", "user not found"); 
    return; 
  }

  if (idx < (int)STATIC_COUNT) {
    User& u = staticUsers[idx];
    if (name.length() > 0) u.name = name;
    if (creditStr.length() > 0) u.credit = creditStr.toInt();
    if (inStr.length() > 0) u.in = (inStr == "true" || inStr == "1");
    saveStaticState(idx);
    logEvent("Update static user " + u.uid + " (" + u.name + ")");
    sendJSON("{\"success\":true,\"user\":{\"uid\":\""+u.uid+"\",\"name\":\""+jsonEscape(u.name)+"\",\"credit\":"+String(u.credit)+",\"in\":"+(u.in?"true":"false")+"}}");
    return;
  }
  
  int di = idx - (int)STATIC_COUNT;
  if (di < 0 || di >= (int)dynamicUsers.size()) { 
    server.send(404, "text/plain", "user not found"); 
    return; 
  }

  User& u = dynamicUsers[di];
  bool immediate = false;
  
  if (name.length() > 0) u.name = name;
  if (creditStr.length() > 0) { 
    u.credit = creditStr.toInt(); 
    immediate = true; 
  }
  if (inStr.length() > 0) u.in = (inStr == "true" || inStr == "1");
  
  markDynamicDirty(immediate);
  logEvent("Update dynamic user " + u.uid + " (" + u.name + ")");
  sendJSON("{\"success\":true,\"user\":{\"uid\":\""+u.uid+"\",\"name\":\""+jsonEscape(u.name)+"\",\"credit\":"+String(u.credit)+",\"in\":"+(u.in?"true":"false")+"}}");
}

void handleEvents(){
  String j = "{\"events\":[";
  for (size_t i=0;i<events.size();i++){ if (i) j += ","; j += "{\"t\":"+String(events[i].t)+",\"msg\":\""+jsonEscape(events[i].msg)+"\"}"; }
  j += "]}"; sendJSON(j);
}

void handleUsersExport(){
  server.sendHeader("Content-Disposition", "attachment; filename=users.json");
  server.sendHeader("Cache-Control", "no-cache");
  String j="["; bool first=true;
  auto pushUser=[&](const User& u){
    if(!first) j+=","; first=false;
    j+="{\"uid\":\""+u.uid+"\",\"name\":\""+jsonEscape(u.name)+"\",\"credit\":"+String(u.credit)+",\"in\":"+(u.in?String("true"):String("false"))+",\"type\":\""+String(u.type==U_STA?"STA":"DYN")+"\"}";
  };
  for (size_t i=0;i<STATIC_COUNT;i++) pushUser(staticUsers[i]);
  for (auto& u: dynamicUsers) pushUser(u);
  j+="]";
  server.send(200, "application/json; charset=utf-8", j);
}

void handleSelftest(){
  bool rc522 = true, oled = true, led = true, servo = true, nvs = true;
  String j = String("{\"rc522\":")+(rc522?"true":"false")
            +",\"oled\":"+(oled?"true":"false")
            +",\"led\":"+(led?"true":"false")
            +",\"servo\":"+(servo?"true":"false")
            +",\"nvs\":"+(nvs?"true":"false")
            +"}";
  sendJSON(j);
}

void handleTimeSync(){
  long timestamp = 0;
  if (server.hasArg("timestamp")) timestamp = server.arg("timestamp").toInt();
  if (isJsonRequest()){
    String body = getJsonBody();
    long parsed = 0;
    if (jsonValueLong(body, "timestamp", parsed)) timestamp = parsed;
  }

  if (timestamp > 0) {
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    
    time_t now = time(nullptr);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", 
             tm_info.tm_year+1900, tm_info.tm_mon+1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
    
    logEvent("Time synced: " + String(buf));
    sendJSON("{\"success\":true,\"time\":\""+String(buf)+"\"}");
  } else {
    sendJSON("{\"success\":false,\"error\":\"invalid timestamp\"}");
  }
}

void handleDatabaseSync(){
  if (server.hasHeader("Content-Type") && server.header("Content-Type").indexOf("application/json") >= 0) {
    String body = server.arg("plain");
    
    // Clear existing dynamic users
    dynamicUsers.clear();
    
    // Parse users array from JSON
    int usersStart = body.indexOf("\"users\"");
    if (usersStart >= 0) {
      int arrayStart = body.indexOf("[", usersStart);
      int arrayEnd = body.indexOf("]", arrayStart);
      
      if (arrayStart >= 0 && arrayEnd > arrayStart) {
        String usersJson = body.substring(arrayStart + 1, arrayEnd);
        int pos = 0;
        
        while (pos < (int)usersJson.length()) {
          int objStart = usersJson.indexOf("{", pos);
          if (objStart < 0) break;
          
          int objEnd = usersJson.indexOf("}", objStart);
          if (objEnd < 0) break;
          
          String userObj = usersJson.substring(objStart, objEnd + 1);
          
          String uid = jsonValue(userObj, "uid");
          String name = jsonValue(userObj, "name");
          long credit = DEFAULT_CREDIT;
          long parsedCredit = 0;
          if (jsonValueLong(userObj, "credit", parsedCredit)) credit = parsedCredit;
          bool in = false;
          bool inVal = false;
          if (jsonValueBool(userObj, "in", inVal)) in = inVal;
          
          // Add user if valid
          if (uid.length() > 0 && name.length() > 0) {
            User u;
            u.uid = normalizeUID(uid);
            u.name = name;
            u.credit = credit;
            u.in = in;
            u.type = U_DYN;
            
            if (u.uid != "" && !uidExistsExcept(u.uid, -1)) {
              dynamicUsers.push_back(u);
            }
          }
          
          pos = objEnd + 1;
        }
      }
    }
    
    // Save to NVS
    markDynamicDirty(true);
    
    logEvent("Database synced: " + String(dynamicUsers.size()) + " users");
    sendJSON("{\"success\":true,\"users\":"+String(dynamicUsers.size())+"}");
  } else {
    sendJSON("{\"success\":false,\"error\":\"JSON required\"}");
  }
}

// Get last input scan for admin panel input mode
void handleLastInput(){
  String j = "{";
  j += "\"hasInput\":" + String(LastScan.uid.length() > 0 ? "true" : "false") + ",";
  j += "\"uid\":\"" + LastScan.uid + "\",";
  j += "\"timestamp\":" + String(LastScan.timestamp) + ",";
  j += "\"isNew\":" + String(LastScan.isNew ? "true" : "false") + ",";
  j += "\"inputMode\":" + String(InputMode.active ? "true" : "false");
  j += "}";
  
  sendJSON(j);
  
  // Clear the last scan after reading to prevent duplicate processing
  if (server.hasArg("clear") && server.arg("clear") == "true") {
    LastScan.uid = "";
    LastScan.timestamp = 0;
    LastScan.isNew = false;
  }
}

// ================== Gate mechanics ==================
void gateOpen() { 
  gateServo.write(GATE_OPEN_DEG); 
  gateCloseAtMs = millis() + GATE_OPEN_MS; 
  gateIsOpen = true;
  logEvent("Gate opened"); 
}
void gateMaybeClose(){ 
  if (gateCloseAtMs && millis() >= gateCloseAtMs) { 
    gateServo.write(GATE_CLOSED_DEG); 
    gateCloseAtMs = 0; 
    gateIsOpen = false;
    logEvent("Gate closed"); 
  } 
}

// ================== Static persistence ==================
void saveStaticState(size_t idx) {
  char keyC[16], keyS[16], keyN[16];
  snprintf(keyC, sizeof keyC, "s%u_cred", (unsigned)idx);
  snprintf(keyS, sizeof keyS, "s%u_in",   (unsigned)idx);
  snprintf(keyN, sizeof keyN, "s%u_name", (unsigned)idx);
  prefs.putInt(keyC, (int)staticUsers[idx].credit);
  prefs.putBool(keyS, staticUsers[idx].in);
  prefs.putString(keyN, staticUsers[idx].name);
}
void loadStaticState(size_t idx) {
  char keyC[16], keyS[16], keyN[16];
  snprintf(keyC, sizeof keyC, "s%u_cred", (unsigned)idx);
  snprintf(keyS, sizeof keyS, "s%u_in",   (unsigned)idx);
  snprintf(keyN, sizeof keyN, "s%u_name", (unsigned)idx);
  if (prefs.isKey(keyC)) staticUsers[idx].credit = prefs.getInt(keyC, (int)staticUsers[idx].credit);
  if (prefs.isKey(keyS)) staticUsers[idx].in     = prefs.getBool(keyS, staticUsers[idx].in);
  if (prefs.isKey(keyN)) staticUsers[idx].name   = prefs.getString(keyN, staticUsers[idx].name);
}

// ================== Setup / Loop ==================
void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { Serial.println("OLED init failed"); while (true) delay(100); }

  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println("Gate System");
  display.drawLine(0, 10, OLED_W, 10, SSD1306_WHITE);
  display.setCursor(0, 24); display.println("Initializing..."); display.display();

  gateServo.attach(SERVO_PIN); gateServo.write(GATE_CLOSED_DEG);
  pinMode(LED_R_PIN, OUTPUT); pinMode(LED_G_PIN, OUTPUT); pinMode(LED_B_PIN, OUTPUT); setLED(false,false,false);

  SPI.begin(18, 19, 23, RC522_SS); rfid.PCD_Init(); delay(50);

  connectWiFi(); setupTime();

  prefs.begin(NVS_NS, false);
  for (size_t i=0; i<STATIC_COUNT; ++i) loadStaticState(i);
  loadDynamicUsers(); // restore dynamic users

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/info",          HTTP_GET, handleInfo);
  server.on("/api/state",         HTTP_GET, handleState);
  server.on("/api/open",          HTTP_POST, handleOpen);
  server.on("/api/led",           HTTP_POST, handleLED);
  server.on("/api/input/mode",    HTTP_POST, handleInputMode);
  server.on("/api/users/add",     HTTP_POST, handleUsersAdd);
  server.on("/api/users/update",  HTTP_POST, handleUsersUpdate);
  server.on("/api/users/delete",  HTTP_POST, handleUsersDelete);
  server.on("/api/users/clear",   HTTP_POST, handleUsersClear);
  server.on("/api/credit/add",    HTTP_POST, handleCreditAdd);
  server.on("/api/state/set",     HTTP_POST, handleStateSet);
  server.on("/api/events",        HTTP_GET,  handleEvents);
  server.on("/api/users/export",  HTTP_GET,  handleUsersExport);
  server.on("/api/selftest",      HTTP_GET,  handleSelftest);
  server.on("/api/time/sync",     HTTP_POST, handleTimeSync);
  server.on("/api/database/sync", HTTP_POST, handleDatabaseSync);
  server.on("/api/input/last",    HTTP_GET,  handleLastInput);
  server.begin();

  showIdle();
  logEvent("Boot "+String(FW_VERSION));
}

void loop() {
  server.handleClient();
  gateMaybeClose();

  // Debounced dynamic save
  if (dynDirty && millis() - dynDirtyAt >= DYN_SAVE_DELAY_MS) {
    saveDynamicUsers(); dynDirty = false;
  }

  if (showing) {
    if (millis() >= showUntilMs) { showing = false; showIdle(); }
    delay(10);
    return;
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(30);
    return;
  }

  String uid = uidToHex(rfid.uid);
  MFRC522::PICC_Type type = rfid.PICC_GetType(rfid.uid.sak);

  int idxCombined = findUserIndexCombined(uid);
  bool isStatic = (idxCombined >= 0 && idxCombined < (int)STATIC_COUNT);

  drawHeaderWithClock("Gate System");
  display.fillRect(0, 22, OLED_W, OLED_H-22, SSD1306_BLACK);
  display.setCursor(0, 24); display.print("UID: "); display.println(uid);

  if (idxCombined >= 0) {
    // Known user -> normal gate logic
    User* uptr = nullptr;
    if (isStatic) uptr = &staticUsers[idxCombined];
    else { int di = idxCombined - (int)STATIC_COUNT; uptr = &dynamicUsers[di]; }
    User& u = *uptr;

    bool nowIn = !u.in;
    bool allow = true;

    if (!nowIn) { // exit charges
      if (u.credit >= COST_PER_EXIT) {
        u.credit -= COST_PER_EXIT;
        if (isStatic) saveStaticState(idxCombined);
        else markDynamicDirty(true); // financial -> immediate
      } else {
        allow = false;
      }
    }

    if (allow) {
      u.in = nowIn;
      if (isStatic) saveStaticState(idxCombined);
      else markDynamicDirty(false); // debounced

      ledAccessGranted();
      display.setCursor(0, 36); display.print("User: "); display.println(u.name);
      display.setCursor(0, 48); display.print("Access: GRANTED ("); display.print(nowIn ? "IN" : "OUT"); display.println(")");
      display.setCursor(0, 56); display.print("Balance: "); display.println(vndFormat(u.credit));
      display.display();

      Serial.printf("UID: %s\n", uid.c_str());
      Serial.printf("Type: %s\n", MFRC522::PICC_GetTypeName(type));
      Serial.println("Access: GRANTED");
      Serial.printf("User: %s, IN=%d, Credit=%ld\n", u.name.c_str(), (int)u.in, u.credit);
      logEvent("Access granted - UID: "+uid+" ("+(nowIn?"IN":"OUT")+") Balance: "+vndFormat(u.credit)+" User: "+u.name);

      gateOpen();
    } else {
      ledAccessDenied();
      display.setTextSize(2); display.setCursor(0, 30); display.println("DENIED");
      display.setTextSize(1); display.setCursor(0, 48); display.print("Not enough credit"); display.display();

      Serial.printf("UID: %s\n", uid.c_str());
      Serial.printf("Type: %s\n", MFRC522::PICC_GetTypeName(type));
      Serial.println("Access: DENIED (Insufficient credit)");
      Serial.printf("User: %s, IN=%d, Credit=%ld\n", u.name.c_str(), (int)u.in, u.credit);
      logEvent("Access denied - UID: "+uid+" (insufficient credit) Balance: "+vndFormat(u.credit)+" User: "+u.name);

      if (WiFi.status()==WL_CONNECTED){
        HTTPClient http; http.begin(ADMIN_ALERT_URL); http.addHeader("Content-Type", "application/json");
        String body = "{\"event\":\"INSUFFICIENT_CREDIT\",\"uid\":\""+uid+"\",\"name\":\""+jsonEscape(u.name)+"\",\"credit\":"+String(u.credit)+",\"reason\":\"EXIT_DENIED_INSUFFICIENT_CREDIT\"}";
        int code = http.POST(body); Serial.printf("Admin alert POST -> HTTP %d\n", code); http.end();
      }
    }

  } else {
    // Unknown tag - Enhanced for Input Mode compatibility
    if (InputMode.active) {
      // Enhanced logging for input mode detection
      Serial.printf("SCAN DETECTED - UID: %s\n", uid.c_str());
      Serial.printf("Type: %s\n", MFRC522::PICC_GetTypeName(type));
      
      // Update last scan tracking
      LastScan.uid = uid;
      LastScan.timestamp = millis() / 1000;
      LastScan.isNew = true; // Always mark as new for input mode
      
      logEvent("Card scanned - UID: "+uid+" (Input Mode Active)");
      
      ledAccessGranted();
      display.setTextSize(1);
      display.setCursor(0, 36); display.println("CARD DETECTED");
      display.setCursor(0, 46); display.println("UID: " + uid);
      display.setCursor(0, 56); display.println("Ready for registration");
      display.display();
      
      Serial.printf("Card detected in Input Mode: %s\n", uid.c_str());
      logEvent("New card scan - UID: "+uid+" (awaiting admin registration)");
    } else {
      ledAccessDenied();
      display.setTextSize(2); display.setCursor(0, 30); display.println("INVALID");
      display.setTextSize(1); display.setCursor(0, 48); display.println("Access: DENIED"); display.display();
      Serial.printf("Unknown UID: %s\n", uid.c_str());
      logEvent("Unknown card - UID: "+uid+" (access denied)");
    }
  }

  showing = true;
  showUntilMs = millis() + 3000UL;

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
