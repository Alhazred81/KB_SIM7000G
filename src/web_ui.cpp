// web_ui.cpp

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "web_ui.h"
#include "web_common.h"
#include "web_hives.h"
#include "web_config.h"
#include "sensors.h"
#include "modem_mgr.h"
#include "gnss_mgr.h"
#include "weather_mgr.h"
#include "calendar.h"
#include "time_mgr.h"

extern WebServer server;
extern String htmlHead(const String& title, const String& activeTab);
extern String htmlFoot();
extern String gApSSID;

// --- Külső modulokból származó handler függvények deklarációi ---

// Főoldalak
extern void handleHives();
extern void handleCfg();
extern void handleGsm();
extern void handleIot();
extern void handleGnss();
extern void handleSensors();
extern void handleDiag();
extern void handleExpert();
extern void handleHiveView();

// GSM modul
extern void handleDoSms();
extern void handleSmsStatus();
extern void handleDoCall();
extern void handleHangup();
extern void handleSetSmsc();
extern void handleNetAuto();
extern void handleNetScan();
extern void handleNetManual();

// IoT modul
extern void handleDataOn();
extern void handleDataOff();
extern void handleDataPing();
extern void handleNtfySend();
extern void handleNtfyPoll();
extern void handleSaveReport();
extern void handleTestReport();

// Backup modul
extern void handleEepromBackup();
extern void handleEepromRestore();

// Szenzor modul
extern void handleSensConfig();
extern void handleSensToggle();
extern void handleSensStatus();
extern void handleSensTest();

// Diag & Rendszer modul
extern void handleAtAjax();
extern void handleAtStatus();
extern void handleModemStatus();
extern void handleReinit();
extern void handleExpertPost();
extern void handleExpertReset();
extern void handleExpertFullReset();
extern void handleEspRestart();

// GNSS modul
extern void handleGnssStatus();
extern void handleGnssAssist();
extern void handleGnssCtl();

// Egyéb
extern void handleSaveWifi();
extern void handleSaveNtfy();
extern void handleSaveWeatherCfg();
extern void handleTestWeatherAlert();
extern void handleMapStatusApi();
extern void handleEvaluate();
extern void handleEvaluatePost();
extern void handleConfig();
extern void handleConfigPost();
extern void handleRegisterPart();
extern void handleRegisterPartPost();
extern void handleWifiScan();
extern void handleStaConnect();
extern void handleStaDisconnect();

// --- Főoldal / Műszerfal nézet ---
void handleRoot() {
  if (!checkPinGuard()) return;

  String html = htmlHead("Főoldal", "1");

  // Reszponzív Grid elrendezés a 4 fő kártyának
  html += "<div style='display:grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 16px; width:100%; margin-bottom: 16px;'>";

  // 1. GSM / SIM Státusz
  html += "<div class='card' style='margin:0; height:100%; display:flex; flex-direction:column;'>";
  html += "<h2>📶 GSM & SIM Státusz</h2>";
  html += "<div style='flex:1;'>";
  html += stateRow("Modem", gModem.ready ? "Kész" : "Inicializálás...", gModem.ready ? "g" : "y");
  html += stateRow("Hálózat", gModem.registered ? "Csatlakozva" : "Keresés / Offline", gModem.registered ? "g" : "r");
  if(gModem.registered) {
    html += stateRow("Operátor", gModem.operatorName, "");
    html += stateRow("Térerő", String(gModem.signalQuality) + " / 31", gModem.signalQuality > 10 ? "g" : "y");
    html += stateRow("Adatkapcsolat", gData.active ? "Aktív" : "Inaktív", gData.active ? "g" : "y");
  }
  html += "</div>";
  html += "<a href='/gsm' style='margin-top:auto;'><button class='sec'>Részletek</button></a>";
  html += "</div>";

  // 2. GNSS Státusz
  html += "<div class='card' style='margin:0; height:100%; display:flex; flex-direction:column;'>";
  html += "<h2>🛰 GNSS Státusz</h2>";
  html += "<div style='flex:1;'>";
  html += stateRow("Vevő modul", gGnss.enabled ? "Bekapcsolva" : "Kikapcsolva", gGnss.enabled ? "g" : "r");
  if(gGnss.enabled) {
    html += stateRow("Műholdas Fix", gGnss.fix ? "Van (3D)" : "Keresés...", gGnss.fix ? "g" : "y");
    if(gGnss.fix) {
      html += stateRow("Használt Műholdak", String(gGnss.satUsed) + " db", "g");
      html += stateRow("HDOP (Pontosság)", String(gGnss.hdop, 1), gGnss.hdop < 2.5 ? "g" : "y");
    }
  }
  html += "</div>";
  html += "<a href='/gnss' style='margin-top:auto;'><button class='sec'>Részletek</button></a>";
  html += "</div>";

  // 3. Szenzorok (Kompakt)
  html += "<div class='card' style='margin:0; height:100%; display:flex; flex-direction:column;'>";
  html += "<h2>🌡 Elérhető Szenzorok</h2>";
  html += "<div style='flex:1;'>";
  bool anySensor = false;
  
  if(gSht.enabled && gSht.lastGoodRead > 0) {
    html += stateRow("Belső Hő/Pára", String(gSht.tempC, 1) + " °C | " + String(gSht.humidityPct, 0) + " %", "g");
    anySensor = true;
  }
  if(gWindSpeed.enabled && gWindSpeed.lastGoodRead > 0) {
    html += stateRow("Szélsebesség", String(gWindSpeed.speedMs, 1) + " m/s", "g");
    anySensor = true;
  }
  if(gWindDir.enabled && gWindDir.lastGoodRead > 0) {
    html += stateRow("Szélirány", String(gWindDir.directionDeg, 0) + "°", "g");
    anySensor = true;
  }
  if(gRain.enabled && gRain.lastPoll > 0) {
    html += stateRow("Csapadék", String(gRain.percentWet) + " % " + (gRain.isRaining ? "(Esik)" : ""), gRain.isRaining ? "y" : "g");
    anySensor = true;
  }
  if(gAhtBmp.enabled && gAhtBmp.lastGoodRead > 0) {
    String t = gAhtBmp.ahtOk ? (String(gAhtBmp.ahtTempC, 1) + " °C") : "N/A";
    String p = gAhtBmp.bmpOk ? (String(gAhtBmp.bmpPressureHpa, 0) + " hPa") : "N/A";
    html += stateRow("Külső Hő/Légnyom.", t + " | " + p, "g");
    anySensor = true;
  }
  if(gLtr.enabled && gLtr.lastGoodRead > 0) {
    html += stateRow("UV Index", String(gLtr.uvIndex, 1), "g");
    anySensor = true;
  }
  if(gMpu.enabled && gMpu.lastGoodRead > 0) {
    html += stateRow("Mérleg Dőlés", "Aktív", "g");
    anySensor = true;
  }
  if(!anySensor) {
    html += "<p class='hint' style='margin-top:10px;'>Nincs aktív vagy friss szenzor adat.</p>";
  }
  html += "</div>";
  html += "<a href='/sensors' style='margin-top:auto;'><button class='sec'>Összes szenzor</button></a>";
  html += "</div>";

  // 4. Időjárás és Naptár
  html += "<div class='card' style='margin:0; height:100%; display:flex; flex-direction:column;'>";
  html += "<h2>🌤 Időjárás & Rendszeridő</h2>";
  html += "<div style='flex:1;'>";
  
  html += stateRow("Rendszeridő", gTime.synced ? gTime.localTime : "Nincs szinkron", gTime.synced ? "g" : "r");
  html += "<hr style='border:0; border-top:1px solid var(--border); margin:12px 0;'>";

  if(gWeatherHasData) {
    float dayMin = 99.0, dayMax = -99.0, dayPrecip = 0.0;
    for(int b=0; b<4; b++) {
      if(gForecast[0].blocks[b].tempMin < dayMin) dayMin = gForecast[0].blocks[b].tempMin;
      if(gForecast[0].blocks[b].tempMax > dayMax) dayMax = gForecast[0].blocks[b].tempMax;
      dayPrecip += gForecast[0].blocks[b].precip;
    }
    html += stateRow("Mai Időjárás", String(dayMin, 1) + "°C - " + String(dayMax, 1) + "°C", "");
    html += stateRow("Csapadék (Ma)", String(dayPrecip, 1) + " mm", dayPrecip > 0.0 ? "y" : "");
    html += stateRow("Előrejelzés állapota", ageText(gLastWeatherSync) + " frissítve", "dim");
  } else {
    html += "<p class='hint'>Nincs időjárás adat. Várakozás GPS fixre és időre...</p>";
  }
  html += "</div>";
  html += "</div>"; // Kártya vége

  html += "</div>"; // Grid vége

  // Naptár nézet beszúrása alulra
  html += getCalendarCardHtml();

  html += htmlFoot();
  server.send(200, "text/html", html);
}

// --- Útvonalak regisztrálása ---
void webBegin() {
  server.on("/", handleRoot);
  server.on("/s.css", handleCss); 
  server.on("/hives", handleHives);
  server.on("/cfg", handleCfg);
  server.on("/gsm", handleGsm);
  server.on("/iot", handleIot);
  server.on("/gnss", handleGnss);
  server.on("/sensors", handleSensors);
  server.on("/diag", handleDiag);
  server.on("/expert", handleExpert);
  server.on("/hive", handleHiveView);

  // GSM végpontok
  server.on("/dosms", HTTP_POST, handleDoSms);
  server.on("/smsstatus", handleSmsStatus);
  server.on("/docall", HTTP_POST, handleDoCall);
  server.on("/hangup", HTTP_POST, handleHangup);
  server.on("/setsmsc", HTTP_POST, handleSetSmsc);
  server.on("/netauto", HTTP_POST, handleNetAuto);
  server.on("/netscan", HTTP_POST, handleNetScan);
  server.on("/netmanual", HTTP_POST, handleNetManual);

  // IoT / Ntfy / EEPROM végpontok
  server.on("/dataon", HTTP_POST, handleDataOn);
  server.on("/dataoff", HTTP_POST, handleDataOff);
  server.on("/dataping", HTTP_POST, handleDataPing);
  server.on("/ntfy-send", HTTP_POST, handleNtfySend);
  server.on("/ntfy-poll", HTTP_POST, handleNtfyPoll);
  server.on("/save-report", HTTP_POST, handleSaveReport);
  server.on("/test-report", HTTP_POST, handleTestReport);
  server.on("/eeprombackup", handleEepromBackup);
  server.on("/eepromrestore", HTTP_POST, handleEepromRestore);

  // Szenzor kapcsolók és teszt végpontok
  server.on("/sensconfig", HTTP_POST, handleSensConfig);
  server.on("/senstoggle", HTTP_POST, handleSensToggle);
  server.on("/sensstatus", HTTP_GET, handleSensStatus);
  server.on("/senstest", HTTP_POST, handleSensTest);

  // Diag & Expert végpontok
  server.on("/at_ajax", handleAtAjax);
  server.on("/atstatus", HTTP_POST, handleAtStatus);
  server.on("/modemstatus", handleModemStatus);
  server.on("/reinit", HTTP_POST, handleReinit);
  server.on("/expertpost", HTTP_POST, handleExpertPost);
  server.on("/expertreset", handleExpertReset);
  server.on("/expertfullreset", HTTP_POST, handleExpertFullReset);
  server.on("/esprestart", HTTP_POST, handleEspRestart);

  // GNSS végpontok
  server.on("/gnssstatus", handleGnssStatus);
  server.on("/gnssassist", HTTP_POST, handleGnssAssist);
  server.on("/gnssctl", HTTP_POST, handleGnssCtl);

  // Egyéb funkciók
  server.on("/savewifi", HTTP_POST, handleSaveWifi);
  server.on("/save-ntfy", HTTP_POST, handleSaveNtfy);
  server.on("/saveweathercfg", HTTP_POST, handleSaveWeatherCfg);
  server.on("/testweatheralert", HTTP_POST, handleTestWeatherAlert);
  server.on("/api/map_status", HTTP_GET, handleMapStatusApi);
  server.on("/evaluate", handleEvaluate);
  server.on("/evaluate_post", HTTP_POST, handleEvaluatePost);
  server.on("/config", handleConfig);
  server.on("/config_post", HTTP_POST, handleConfigPost);
  server.on("/register_part", handleRegisterPart);
  server.on("/register_part_post", HTTP_POST, handleRegisterPartPost);
  server.on("/wifiscan", handleWifiScan);
  server.on("/staconnect", HTTP_POST, handleStaConnect);
  server.on("/stadisconnect", HTTP_POST, handleStaDisconnect);
  
  // A LittleFS-en lévő fájlok kiszolgálása
  server.serveStatic("/", LittleFS, "/");

  server.begin();
}