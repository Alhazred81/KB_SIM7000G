/*
 * KB SIM7000G vezérlő
 * ===================
 * Board    : ESP32 Dev Module (Arduino IDE)
 *            vagy platform=espressif32 / board=esp32dev (PlatformIO)
 * Könyvtár : TinyGSM (vshymanskyy/TinyGSM) >= 0.11.7
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "config.h"
#include "crypto.h"
#include "gnss_mgr.h"
#include "modem_mgr.h"
#include "NtfyClient.h"
#include "sensors.h"
#include "time_mgr.h"
#include "web_ui.h"
#include "wifi_sta.h"



// ─── Globálisok ─────────────────────────────────────────────

extern String gReportTimes;
void checkAndSendScheduledReport();
String loadReportConfig();
HardwareSerial modemSerial(1);
TinyGsm        modem(modemSerial);
bool gStartupNtfySent = false;

// --- ÚJ: NtfyClient példányosítása ---
// Átadjuk a modem soros portját, a topic nevét, és a szervert.
NtfyClient     ntfy(modemSerial, "kb_sim7000g_balazs", "ntfy.sh");


WebServer      server(80);
DNSServer      dnsServer;

ModemState     gModem;
LedConfig      gLed;
GnssState      gGnss;
DataConnState  gData;
WindSpeedState gWindSpeed;
WindDirState   gWindDir;
ShtSensorState gSht;
RainSensorState gRain;
Mpu6050State    gMpu;
Aht20Bmp280State gAhtBmp;
Ltr390State      gLtr;
String         gApSSID    = "";
String         gApPass    = DEFAULT_AP_PASS;
uint8_t        gApChannel = DEFAULT_CHANNEL;
unsigned long  gLastSms   = 0;
bool           gDiagEnabled = true;

bool           gModemInitRequested = false;

bool           gSmsSendRequested = false;
String         gSmsPendingNum    = "";
String         gSmsPendingText   = "";
bool           gSmsSendInProgress = false;
bool           gSmsSendDone       = false;
String         gSmsSendResult     = ""; 
String         loadReportConfig();
extern String gReportTimes;

void loadSmsInboxLimit() {
  // Ha EEPROM-ból olvasod, itt kell betölteni, 
  // ha fix érték, akkor adhatsz vissza egy alapértelmezettet is:
}

String macSuffix() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  char s[7]; snprintf(s, sizeof(s), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(s);
}

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(gApSSID.c_str(), gApPass.c_str(), gApChannel);
  Serial.println("[AP] SSID: " + gApSSID);
  Serial.println("[AP] IP:   " + WiFi.softAPIP().toString());
  Serial.println("[AP] CH:   " + String(gApChannel));
}

enum class LedPhase {
  NET_ON, NET_GAP1, NET_GAP2, NET_OFF_PAUSE,
  GNSS_ON, GNSS_GAP1, GNSS_GAP2, GNSS_OFF_PAUSE,
  LONG_PAUSE
};
static LedPhase ledPhase = LedPhase::NET_ON;
static unsigned long ledPhaseStart = 0;
static uint8_t ledBlinkStep = 0; 

void updateLED() {
  if(gLed.mode == LED_MODE_AT_NETLIGHT) return;

  int pin = currentLedGpio();
  if(pin < 0) return;

  if(gLed.manualOverride) {
    digitalWrite(pin, gLed.triggerOn ? HIGH : LOW);
    return;
  }

  if(!gModem.powered || (gModem.initAttempts > 0 && !gModem.uartResponding)) {
    unsigned long now = millis();
    static unsigned long fallbackT = 0;
    static bool fallbackS = false;
    if(now - fallbackT >= 150) {
      fallbackS = !fallbackS;
      digitalWrite(pin, fallbackS);
      fallbackT = now;
    }
    return;
  }

  bool netOk  = gModem.ready && gModem.registered;
  bool gnssOk = gGnss.enabled && gGnss.fix;

  unsigned long now = millis();
  unsigned long elapsed = now - ledPhaseStart;

  auto nextPhase = [&](LedPhase p){ ledPhase = p; ledPhaseStart = now; ledBlinkStep = 0; };

  switch(ledPhase) {
    case LedPhase::NET_ON:
      digitalWrite(pin, HIGH);
      if(elapsed >= 150) {
        digitalWrite(pin, LOW);
        if(netOk) {
          nextPhase(LedPhase::NET_OFF_PAUSE);
        } else {
          nextPhase(LedPhase::NET_GAP1);
        }
      }
      break;
    case LedPhase::NET_GAP1:
      if(elapsed >= 120) nextPhase(LedPhase::NET_GAP2);
      break;
    case LedPhase::NET_GAP2:
      digitalWrite(pin, HIGH);
      if(elapsed >= 120 + 150) {
        digitalWrite(pin, LOW);
        nextPhase(LedPhase::NET_OFF_PAUSE);
      }
      break;
    case LedPhase::NET_OFF_PAUSE:
      if(elapsed >= 300) nextPhase(LedPhase::GNSS_ON);
      break;

    case LedPhase::GNSS_ON:
      digitalWrite(pin, HIGH);
      if(elapsed >= 150) {
        digitalWrite(pin, LOW);
        if(gnssOk) {
          nextPhase(LedPhase::GNSS_OFF_PAUSE);
        } else {
          nextPhase(LedPhase::GNSS_GAP1);
        }
      }
      break;
    case LedPhase::GNSS_GAP1:
      if(elapsed >= 120) nextPhase(LedPhase::GNSS_GAP2);
      break;
    case LedPhase::GNSS_GAP2:
      digitalWrite(pin, HIGH);
      if(elapsed >= 120 + 150) {
        digitalWrite(pin, LOW);
        nextPhase(LedPhase::GNSS_OFF_PAUSE);
      }
      break;
    case LedPhase::GNSS_OFF_PAUSE:
      if(elapsed >= 300) nextPhase(LedPhase::LONG_PAUSE);
      break;

    case LedPhase::LONG_PAUSE:
      if(elapsed >= 1000) nextPhase(LedPhase::NET_ON);
      break;
  }
}

void handleSerial() {
  if(!Serial.available()) return;
  Serial.setTimeout(50); 
  String cmd = Serial.readStringUntil('\n'); cmd.trim();
  if(cmd.length()==0) return;
  Serial.println(">> " + cmd);

  if(cmd == "status") {
    Serial.println("Modem ready:   " + String(gModem.ready));
    Serial.println("Registered:    " + String(gModem.registered));
    Serial.println("Operator:      " + gModem.operatorName);
    Serial.println("Signal:        " + String(gModem.signalQuality));
    Serial.println("NetType:       " + gModem.netType);
    Serial.println("CCID:          " + gModem.simCCID);
    Serial.println("IMEI:          " + gModem.simIMEI);
    Serial.println("Ido/NTP:       " + String(gTime.synced ? "OK " : "nincs ") + gTime.localTime);
    Serial.println("AP SSID:       " + gApSSID);
    Serial.println("AP IP:         " + WiFi.softAPIP().toString());
    if(gSta.mode == NetMode::STA_CONNECTED) {
      Serial.println("STA SSID:      " + gSta.targetSSID);
      Serial.println("STA IP:        " + WiFi.localIP().toString());
      Serial.println("Web URL:       http://" + WiFi.localIP().toString() + "/");
    } else {
      Serial.println("Web URL:       http://" + WiFi.softAPIP().toString() + "/");
    }
    Serial.println("Free heap:     " + String(ESP.getFreeHeap()));
    Serial.println("Uptime:        " + String(millis()/1000) + "s");
  }
  else if(cmd == "reinit") {
    Serial.println("Modem ujraindit...");
    gModem.ready = false;
    bool ok = modemInit();
    Serial.println(ok ? "OK" : "HIBA: " + gModem.lastError);
  }
  else if(cmd == "diag") {
    Serial.println("=== DIAG LOG ===");
    Serial.print(diagDump());
    Serial.println("================");
  }
  else if(cmd.startsWith("at ")) {
    String atcmd = cmd.substring(3);
    modemSerial.println("AT" + atcmd);
    delay(500);
    while(modemSerial.available()) Serial.write(modemSerial.read());
    Serial.println();
  }
  else if(cmd == "help") {
    Serial.println("Parancsok:");
    Serial.println("  status   - modem/halozat allapot");
    Serial.println("  reinit   - modem ujraindit");
    Serial.println("  diag     - esemenynaplo");
    Serial.println("  at <cmd> - AT parancs kuldese (pl: at +CSQ)");
    Serial.println("  help     - ez a lista");
  }
  else {
    Serial.println("Ismeretlen parancs. Ird: help");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== KB SIM7000G indul ==="));
  
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS MOUNT HIBA");
  } else {
    Serial.println("LittleFS OK");
  }
  
  Serial.println("=== LITTLEFS FILES ===");
  Serial.printf("Total bytes: %u\n", LittleFS.totalBytes());
  Serial.printf("Used bytes : %u\n", LittleFS.usedBytes());
  File test = LittleFS.open("/index.html", "r");
  Serial.println(test ? "INDEX OPEN OK" : "INDEX OPEN FAIL");

  File root = LittleFS.open("/");
  File file = root.openNextFile();

  while (file) {
    Serial.println(file.name());
    file = root.openNextFile();
  }
  Serial.println("======================");

  EEPROM.begin(EEPROM_SIZE);

  loadLedConfig();
  ledPinReinit();
  gnssLoadAssist();
  loadSmsInboxLimit();
  loadSensorConfig();
  sensorsApplyEnabled();
  loadNtfyConfig();

  gReportTimes = loadReportConfig();
  gApPass    = loadApPass();
  gApChannel = EEPROM.read(ADDR_CHANNEL);
  if(gApChannel < 1 || gApChannel > 13) gApChannel = DEFAULT_CHANNEL;

  WiFi.mode(WIFI_AP);
  gApSSID = "KB-teszt-" + macSuffix();
  startAP();

  dnsServer.start(53, "*", WiFi.softAPIP());
  diagAdd("Rendszer indul...");
  diagAdd("AP: "+gApSSID);
  webBegin();

  wifiStaTryAutoConnect();

  gModem.bootTime = millis();
  modemPowerOn();
  String pin = loadPin();
  bool ok = modemInit();
  
  // --- ÚJ: ntfy debug engedélyezése ---
  ntfy.setDebugStream(&Serial); // A soros monitorra is kiírja a HTTP kérések eredményét

  if(ok) {
    diagAdd("Modem OK: "+gModem.operatorName);
    gnssStart(); 
    
    // --- ÚJ: Automatikus adatkapcsolat aktiválása hálózatra lépés után ---
    diagAdd("Adatkapcsolat automatikus indítása...");
    dataConnEnable();
        
  } else {
    diagAdd("Modem HIBA: "+gModem.lastError);
  }

  Serial.println(F("=== Kész. Ird 'help' a serial parancsokhoz. ==="));
}

void loop() {
  if(gSta.mode == NetMode::AP || gSta.mode == NetMode::STA_CONNECTING) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
  monitorCall();
  updateModemStats();
  gnssLoop();
  ntpLoop();
  updateLED();
  handleSerial();
  wifiStaLoop();
  wifiStaWatchdog();
  smsInboxLoop();
  sensorsLoop();


  //időzített riport ellenőrzése és küldése
  static unsigned long lastReportCheck = 0;
  if (millis() - lastReportCheck > 15000) { // 15 másodpercenként ellenőrzi
  lastReportCheck = millis();
  checkAndSendScheduledReport();
  }
  // --- ÚJ: Rendszerindítási értesítés küldése NTP és aktív net után ---
  if (!gStartupNtfySent && gTime.synced && gData.active) {
    gStartupNtfySent = true; // Akkor is letiltjuk a további próbálkozást erre a bootra, ha ki van kapcsolva
    
    if (gNtfyStartupMsg) {
      diagAdd("NTP szinkronizálva. Ntfy teszt küldés indítása...");
      bool sent = ntfy.send("A szerver elindult és az idő szinkronizálva van!", "Rendszer Start", NtfyPriority::Default);
      if (sent) {
        diagAdd("Ntfy üzenet sikeresen elküldve!");
      } else {
        diagAdd("Ntfy küldési hiba!");
      }
    } else {
      diagAdd("Indulási ntfy üzenet letiltva a beállításokban.");
    }
  }

  if(gModemInitRequested) {
    gModemInitRequested = false;
    bool ok = modemInit();
    diagAdd(ok ? "Modem init OK" : "Modem init HIBA: " + gModem.lastError);
    if(ok) gnssStart();
  }

  if(gSmsSendRequested) {
    gSmsSendRequested = false;
    gSmsSendInProgress = true;
    gSmsSendDone = false;
    String err = sendSMS(gSmsPendingNum, gSmsPendingText);
    gSmsSendResult = err;
    gSmsSendDone = true;
    gSmsSendInProgress = false;
    if(err.length() == 0) {
      gLastSms = millis();
      diagAdd("SMS OK -> " + gSmsPendingNum + " (" + String(gSmsPendingText.length()) + " kar)");
    } else {
      diagAdd("SMS HIBA -> " + gSmsPendingNum + ": " + err);
    }
  }
}