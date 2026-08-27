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
#include "server_receiver.h"
#include "time_mgr.h"
#include "weather_mgr.h"
#include "web_ui.h"
#include "wifi_sta.h"

// ─── Globálisok ─────────────────────────────────────────────

extern String gReportTimes;
void checkAndSendScheduledReport();
String loadReportConfig();
HardwareSerial modemSerial(1);
TinyGsm        modem(modemSerial);
bool gStartupNtfySent = false;

// NtfyClient példányosítása
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
extern void backgroundTaskLoop();

void loadSmsInboxLimit() {
  // EEPROM betöltés vagy fix érték helye
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
    gModemInitRequested = true; // Lecserélve flagre, hogy ne blokkoljon azonnal
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

// ====================================================================
// SETUP: Villámgyors indulás, csak a kommunikáció és a UI áll fel
// ====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== KB SIM7000G indul ==="));
  
  // 1. Kommunikációs vonalak és memóriák
  // 1024 bájtos buffer a nagy JSON fájloknak!
  modemSerial.setRxBufferSize(1024); 
  // Ha a config.h-ban definiáltad a MODEM_RX és TX pineket, ide beírhatod a begin-be, 
  // ha a modemInit() csinálja, akkor ez csak előkészítés
  
  if (!LittleFS.begin(true)) Serial.println("LittleFS MOUNT HIBA");
  else Serial.println("LittleFS OK");

  EEPROM.begin(EEPROM_SIZE);

  // 2. Beállítások betöltése
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

  // 3. UI és Webszerver elindítása (azonnali elérés)
  WiFi.mode(WIFI_AP);
  gApSSID = "KB-teszt-" + macSuffix();
  startAP();
  dnsServer.start(53, "*", WiFi.softAPIP());
  webBegin();

  wifiStaTryAutoConnect();
  initServerEspNow();
  ntfy.setDebugStream(&Serial);

  // 4. Modulok inicializálása (csak változókat állítanak, nem blokkolnak)
  ntpStart();
  weatherInit();

  // 5. Modem fizikai ébresztése (Power gomb)
  gModem.bootTime = millis();
  modemPowerOn(); 
  
  diagAdd("Rendszer UI elindult. Hatterfolyamatok ebresztese...");
  Serial.println(F("=== Kész. Ird 'help' a serial parancsokhoz. ==="));
  
  // A modem hálózatkeresése majd a loop()-ban indul el!
}

// ====================================================================
// LOOP: Az állapotok karmestere
// ====================================================================
void loop() {
  // A Webszerver SOSEM fagyhat le
  if(gSta.mode == NetMode::AP || gSta.mode == NetMode::STA_CONNECTING) {
    dnsServer.processNextRequest();
  }
  server.handleClient();

  // 1. KÉSLELTETETT MODEM INDÍTÁS (Nem a setup()-ban tartjuk fel a procit)
  static bool startupPhaseDone = false;
  if (!startupPhaseDone && millis() - gModem.bootTime > 2500) {
    startupPhaseDone = true;
    diagAdd("SIM7000G hálózatkeresés indítása a háttérben...");
    gModemInitRequested = true; 
  }

  // Ha kértek újraindítást vagy ez az első boot trigger
  if(gModemInitRequested) {
    gModemInitRequested = false;
    bool ok = modemInit(); // Feltételezve, hogy lélegezteti a webszervert
    diagAdd(ok ? "Modem init OK" : "Modem init HIBA: " + gModem.lastError);
    if(ok) {
      gnssStart(); 
      dataConnEnable();
    }
  }

  // Rendszeres feladatok
  monitorCall();
  updateModemStats();
  
  // Modulok "okos" loopjai (maguktól tudják, hogy várniuk kell-e)
  gnssLoop();
  ntpLoop();
  updateLED();
  handleSerial();
  wifiStaLoop();
  wifiStaWatchdog();
  smsInboxLoop();
  sensorsLoop();
  backgroundTaskLoop(); // Itt indul a weatherUpdate(), ha a gTime.synced == true

  // Időzített riport
  static unsigned long lastReportCheck = 0;
  if (millis() - lastReportCheck > 15000) { 
    lastReportCheck = millis();
    checkAndSendScheduledReport();
  }
  
  // Startup Ntfy értesítés (Megvárja az időszinkront és az aktív netet!)
  if (!gStartupNtfySent && gTime.synced && gData.active) {
    gStartupNtfySent = true; 
    if (gNtfyStartupMsg) {
      diagAdd("NTP szinkronizálva. Ntfy boot teszt küldés indítása...");
      bool sent = ntfy.send("A szerver elindult és az idő szinkronizálva van!", "Rendszer Start", NtfyPriority::Default);
      diagAdd(sent ? "Ntfy boot üzenet sikeresen elküldve!" : "Ntfy küldési hiba!");
    } else {
      diagAdd("Indulási ntfy üzenet letiltva a beállításokban.");
    }
  }

  // SMS Küldés
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