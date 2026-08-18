//wifi_sta.cpp
#include "wifi_sta.h"

// ─── Globális változók definíciója ───────────────────────────
WifiStaState gSta;
ScannedNet gScanResults[MAX_SCAN_RESULTS];
int gScanCount = 0;
unsigned long gLastScan = 0;
unsigned long gLastStaCheck = 0;

// ─── Függvények megvalósítása ────────────────────────────────

void wifiScan() {
  Serial.println(F("[WIFISTA] Halozatok keresese..."));
  int n = WiFi.scanNetworks(false, true); // async=false, hidden=true
  gScanCount = 0;
  for(int i=0; i<n && gScanCount<MAX_SCAN_RESULTS; i++){
    String ssid = WiFi.SSID(i);
    if(ssid.length()==0) continue; // rejtett SSID nevét nem tudjuk kiírni, kihagyjuk
    // Duplikátum-szűrés (több AP ugyanazzal az SSID-vel, csak a legerősebbet tartjuk)
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

  // AP-t egyelőre nem kapcsoljuk le, amíg a STA nem konfirmált
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : NULL);
}

void wifiStaLoop() {
  if(gSta.mode != NetMode::STA_CONNECTING) return;

  wl_status_t st = WiFi.status();

  if(st == WL_CONNECTED){
    gSta.mode = NetMode::STA_CONNECTED;
    gSta.ip = WiFi.localIP().toString();
    Serial.println("[WIFISTA] Csatlakozva! IP: " + gSta.ip);
    Serial.println("[WIFISTA] Web URL: http://" + gSta.ip + "/");
    ntpStart();

    // Mentjük a sikeres hitelesítőket, hogy legközelebb auto-csatlakozzon
    saveStaCreds(gSta.targetSSID, gSta.targetPass);

    // AP leállítása - mostantól tiszta kliens módban vagyunk
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    return;
  }

  // Timeout ellenőrzés
  if(millis() - gSta.connectStarted > STA_CONNECT_TIMEOUT_MS){
    Serial.println(F("[WIFISTA] Csatlakozas idotullepes, vissza AP modba."));
    gSta.mode = NetMode::STA_FAILED;
    gSta.lastError = "Nem sikerult csatlakozni (" + gSta.targetSSID +
                      ") - idotullepes vagy hibas jelszo.";
    gSta.lastAttempt = millis();

    // Visszaállunk tiszta AP módba
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    startAP();
  }
}

void wifiStaTryAutoConnect() {
  String ssid = loadStaSSID();
  if(ssid.length() == 0) return; // nincs mentett STA hálózat
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
}

void wifiStaWatchdog() {
  if(gSta.mode != NetMode::STA_CONNECTED) return;
  if(millis() - gLastStaCheck < 10000) return; // 10 mp-nként elég
  gLastStaCheck = millis();

  if(WiFi.status() != WL_CONNECTED){
    Serial.println(F("[WIFISTA] Kapcsolat elveszett, vissza AP modba."));
    gSta.mode = NetMode::AP;
    gSta.ip = "";
    gSta.lastError = "A WiFi kapcsolat megszakadt (" + gSta.targetSSID + "). Vissza AP modba.";
    WiFi.mode(WIFI_AP);
    startAP();
  }
}