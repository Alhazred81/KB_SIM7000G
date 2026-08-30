#include "wifi_sta.h"
#include "espnow_mgr.h"

WifiStaState gSta;
ScannedNet gScanResults[MAX_SCAN_RESULTS];
int gScanCount = 0;
unsigned long gLastScan = 0;
unsigned long gLastStaCheck = 0;

extern uint8_t gApChannel; // A main.cpp-bol jon

void wifiScan() {
  Serial.println(F("[WIFISTA] Halozatok keresese..."));
  int n = WiFi.scanNetworks(false, true);
  gScanCount = 0;
  for(int i=0; i<n && gScanCount<MAX_SCAN_RESULTS; i++){
    String ssid = WiFi.SSID(i);
    if(ssid.length()==0) continue; 
    
    bool dup = false;
    for(int j=0;j<gScanCount;j++){
      if(gScanResults[j].ssid == ssid){
        dup = true;
        if(WiFi.RSSI(i) > gScanResults[j].rssi) gScanResults[j].rssi = WiFi.RSSI(i);
        break;
      }
    }
    if(dup) continue;
    gScanResults[gScanCount].ssid   = ssid;
    gScanResults[gScanCount].rssi   = WiFi.RSSI(i);
    gScanResults[gScanCount].secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    gScanCount++;
  }
  WiFi.scanDelete();
  gLastScan = millis();
  Serial.println("[WIFISTA] Talalt halozatok: " + String(gScanCount));
}

void wifiStaConnect(const String& ssid, const String& pass) {
  Serial.println("[WIFISTA] Csatlakozas inditasa: " + ssid);
  gSta.targetSSID = ssid;
  gSta.targetPass = pass;
  gSta.mode = NetMode::STA_CONNECTING;
  gSta.connectStarted = millis();
  gSta.lastError = "";

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : NULL);
}

void wifiStaLoop() {
  if(gSta.mode != NetMode::STA_CONNECTING) return;

  wl_status_t st = WiFi.status();

  if(st == WL_CONNECTED){
    gSta.mode = NetMode::STA_CONNECTED;
    gSta.ip = WiFi.localIP().toString();
    Serial.println("[WIFISTA] Csatlakozva. IP: " + gSta.ip);
    
    uint8_t currentChannel = WiFi.channel();
    Serial.printf("[WIFISTA] Aktiv csatorna STA mod utan: %d\n", currentChannel);
    
    initEspNowGateway(currentChannel);
    
    ntpStart();
    saveStaCreds(gSta.targetSSID, gSta.targetPass);

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    return;
  }

  if(millis() - gSta.connectStarted > STA_CONNECT_TIMEOUT_MS){
    Serial.println(F("[WIFISTA] Csatlakozas idotullepes, vissza AP modba."));
    gSta.mode = NetMode::STA_FAILED;
    gSta.lastError = "Nem sikerult csatlakozni (" + gSta.targetSSID +
                     ") - idotullepes vagy hibas jelszo.";
    gSta.lastAttempt = millis();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    startAP();
    
    initEspNowGateway(gApChannel);
  }
}

void wifiStaTryAutoConnect() {
  String ssid = loadStaSSID();
  if(ssid.length() == 0) return; 
  String pass = loadStaPass();
  Serial.println("[WIFISTA] Mentett halozat talalva, csatlakozas: " + ssid);
  wifiStaConnect(ssid, pass);
}

void wifiStaDisconnect() {
  Serial.println(F("[WIFISTA] Kliens mod elhagyasa, vissza AP-ra."));
  clearStaCreds();
  WiFi.disconnect(true);
  gSta.mode = NetMode::AP;
  gSta.targetSSID = "";
  gSta.ip = "";
  WiFi.mode(WIFI_AP);
  startAP();
  
  initEspNowGateway(gApChannel);
}

void wifiStaWatchdog() {
  if(gSta.mode != NetMode::STA_CONNECTED) return;
  if(millis() - gLastStaCheck < 10000) return; 
  gLastStaCheck = millis();

  if(WiFi.status() != WL_CONNECTED){
    Serial.println(F("[WIFISTA] Kapcsolat elveszett, vissza AP modba."));
    gSta.mode = NetMode::AP;
    gSta.ip = "";
    gSta.lastError = "A WiFi kapcsolat megszakadt (" + gSta.targetSSID + "). Vissza AP modba.";
    WiFi.mode(WIFI_AP);
    startAP();
    
    initEspNowGateway(gApChannel);
  }
}

String loadApSSID() {
  String ssid = "";
  for (int i = 0; i < 32; i++) {
    char c = EEPROM.read(ADDR_AP_SSID + i);
    if (c == 0 || c == 255) break;
    ssid += c;
  }
  return ssid;
}

void saveApSSID(const String& ssid) {
  for (int i = 0; i < 32; i++) {
    if (i < ssid.length()) {
      EEPROM.write(ADDR_AP_SSID + i, ssid[i]);
    } else {
      EEPROM.write(ADDR_AP_SSID + i, 0);
    }
  }
  EEPROM.commit();
}

void saveApConfig(const String& ssid, const String& pass, uint8_t channel, bool apHide) {
  for (int i = 0; i < 32; i++) {
    EEPROM.write(ADDR_AP_SSID + i, i < ssid.length() ? ssid[i] : 0);
    EEPROM.write(ADDR_AP_PASS + i, i < pass.length() ? pass[i] : 0);
  }
  EEPROM.write(ADDR_CHANNEL, channel);
  EEPROM.write(ADDR_AP_HIDE, apHide ? 1 : 0);
  EEPROM.commit();
}

bool loadApHide() {
  return EEPROM.read(ADDR_AP_HIDE) == 1;
}

