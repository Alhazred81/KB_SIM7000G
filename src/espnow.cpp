#include "espnow.h"

// Memória a webes tesztablaknak
String gEspNowLog = "";

String getEspNowLog() {
  return gEspNowLog;
}

void clearEspNowLog() {
  gEspNowLog = "";
}

// --- VÉTELI CALLBACK ---
void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_message)) return;

  struct_message myData;
  memcpy(&myData, incomingData, sizeof(myData));

  if (myData.magic != 0xBE) return;

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  // Szerver oldali időbélyeg (Uptime másodpercben) a Deep Sleep ciklus méréséhez
  unsigned long serverTimeSec = millis() / 1000;
  
  // Napló sor összeállítása a webes felületnek
  String logLine = "[" + String(serverTimeSec) + "s] MAC: " + String(macStr) + 
                   " | Ébrenlét: " + String(myData.activeTimeMs) + " ms" +
                   " | Súly: " + String(myData.weight) + " kg" +
                   " | Akku: " + String(myData.vbat) + " V\n";
                   
  Serial.print("[ESP-NOW VÉTEL] " + logLine);

  // Új üzenet a lista tetejére (hogy a legfrissebb legyen legfelül)
  gEspNowLog = logLine + gEspNowLog; 
  
  // Memóriavédelem: ha túl hosszú a log, levágjuk a végét
  if (gEspNowLog.length() > 2000) {
     gEspNowLog = gEspNowLog.substring(0, 2000);
  }
}

// --- KÖZÖS INICIALIZÁLÓ FÜGGVÉNY ---
bool initWiFiAndEspNow(wifi_mode_t mode, String ssid, String pass, uint8_t channel, bool hideSSID) {
  WiFi.mode(mode);
  
  if (mode == WIFI_AP || mode == WIFI_AP_STA) {
    WiFi.softAP(ssid.c_str(), pass.c_str(), channel, hideSSID, 4);
    Serial.printf("[WIFI] AP Indítva: %s | Csatorna: %d\n", ssid.c_str(), channel);
  } else {
    WiFi.channel(channel);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] ❌ Inicializálási hiba!");
    return false;
  }
  
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("[ESP-NOW] ✅ Rádió bekapcsolva, vétel aktív.");
  return true;
}