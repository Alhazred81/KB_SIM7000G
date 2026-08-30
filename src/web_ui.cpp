//web_ui.cpp  

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "web_ui.h"
#include "web_common.h"
#include "web_hives.h"
#include "web_config.h"
#include "web_diag.h"
#include "web_supply.h"
#include "sensors.h"
#include "modem_mgr.h"
#include "gnss_mgr.h"
#include "weather_mgr.h"
#include "calendar.h"
#include "time_mgr.h"

// =================================================================================
// GLOBÁLIS OBJEKTUMOK ÉS VÁLTOZÓK
// =================================================================================
extern WebServer server;
extern String gApSSID;
extern bool checkPinGuard();
extern String htmlHead(const String& title, const String& activeTab);
extern String htmlFoot();

// Globális változó a Setup/Terep módhoz
bool gFieldMode = true;

// =================================================================================
// EXTERN HANDLER DEKLARÁCIÓK TÉMAKÖRÖK SZERINT
// =================================================================================

// --- Alap UI és Rendszer ---
extern void handleCss();
extern void handleCfg();
extern void handleReinit();
extern void handleEspRestart();

// --- Kaptárkezelés, Értékelés és Beavatkozások ---
extern void handleHives();
extern void handleHiveView();
extern void handleEvaluation();
extern void handleTreatment();
extern void handleGetTreatmentsJson();
extern void handleEvaluatePost();
extern void handleConfig();
extern void handleConfigPost();
extern void handleRegisterPart();
extern void handleRegisterPartPost();
extern void handleGetEvaluationsJson();
extern void handleGetColonyFunctionsJson();
extern void handleGetDiseasesJson();
// --- NFC / RFID ---
extern void handleNfc();

// --- Modem és GSM funkciók (Hívás, SMS, Hálózatkeresés) ---
extern void handleGsm();
extern void handleModemStatus();
extern void handleDoSms();
extern void handleSmsStatus();
extern void handleDoCall();
extern void handleHangup();
extern void handleSetSmsc();
extern void handleNetAuto();
extern void handleNetScan();
extern void handleNetManual();

// --- IoT, Adatkapcsolat és Mentések (Ntfy, EEPROM) ---
extern void handleIot();
extern void handleDataOn();
extern void handleDataOff();
extern void handleDataPing();
extern void handleNtfySend();
extern void handleNtfyPoll();
extern void handleSaveNtfy();
extern void handleSaveReport();
extern void handleTestReport();
extern void handleEepromBackup();
extern void handleEepromRestore();

// --- GNSS és Helymeghatározás ---
extern void handleGnss();
extern void handleGnssStatus();
extern void handleGnssAssist();
extern void handleGnssCtl();
extern void handleMapStatusApi();

// --- Szenzorok és Időjárás ---
extern void handleSensors();
extern void handleSensConfig();
extern void handleSensToggle();
extern void handleSensStatus();
extern void handleSensTest();
extern void handleSaveWeatherCfg();
extern void handleTestWeatherAlert();

// --- Wi-Fi Beállítások (STA Hálózat) ---
extern void handleSaveWifi();
extern void handleWifiScan();
extern void handleStaConnect();
extern void handleStaDisconnect();

// --- Diagnosztika és Haladó (AT parancsok, Factory Reset) ---
extern void handleDiag();
extern void handleExpert();
extern void handleAtAjax();
extern void handleAtStatus();
extern void handleExpertPost();
extern void handleExpertReset();
extern void handleExpertFullReset();
extern void handleGetHivesJson();
extern void handleDeleteHive();
extern void handleAddDummyHive();

// --- Kaptár regisztráció ---

// --- Kaptárkezelés --- blokkba:
extern void handleRegStart();
extern void handleRegNfc();
extern void handleRegQueen();
extern void handleRegSurvey();
extern void handleApiSurveyStatus();
extern void handleRegSummary();
extern void handleRegSave();
extern void handleRegCancel();


// =================================================================================
// HELYI HANDLER FÜGGVÉNYEK
// =================================================================================

// --- Módváltó handler (Terep / Setup) ---
void handleSetMode() {
  if (!checkPinGuard()) return;
  if (server.hasArg("m")) {
    gFieldMode = (server.arg("m") == "field");
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// --- Főoldal / Műszerfal nézet (HTML Kód) ---
void handleRoot() {
  if (!checkPinGuard()) return;

  String html = htmlHead("Főoldal", "1");

  html += "<style>"
          ".dot { height: 8px; width: 8px; border-radius: 50%; display: inline-block; margin-right: 6px; vertical-align: middle; }"
          ".dot-g { background-color: #22c55e; box-shadow: 0 0 4px rgba(34,197,94,0.6); }"
          ".dot-y { background-color: #eab308; box-shadow: 0 0 4px rgba(234,179,8,0.6); }"
          ".dot-r { background-color: #ef4444; box-shadow: 0 0 4px rgba(239,68,68,0.6); }"
          ".compact-row { display: flex; align-items: center; justify-content: space-between; padding: 4px 0; border-bottom: 1px solid rgba(255,255,255,0.04); font-size: 12px; }"
          ".dash-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; width: 100%; align-items: start; margin-bottom: 16px; }"
          ".dash-grid > .card { margin: 0 !important; width: 100% !important; box-sizing: border-box; display: flex; flex-direction: column; }"
          "</style>";

  // --- KOMPAKT TEREPMÓD CSÚSZKA ---
  html += "<div style='width: 100%; display:flex; justify-content:space-between; align-items:center; margin-bottom: 16px;'>";
  html += "<div style='font-size:20px; font-weight:bold; color:#fff;'>Műszerfal</div>";
  html += "<div style='display:flex; align-items:center; gap:10px; background:var(--card); padding:6px 16px; border-radius:20px; border:1px solid var(--border); box-shadow: 0 4px 6px rgba(0,0,0,0.3);'>";
  html += "<span style='font-size:13px; font-weight:bold; color:" + String(gFieldMode ? "#22c55e" : "var(--txt2)") + ";'>🌱 Terep</span>";
  html += "<label class='sens-toggle' style='margin:0; --sens-color:#eab308;'>"; 
  html += "<input type='checkbox' onchange=\"location.href='/setmode?m='+(this.checked?'setup':'field')\" " + String(!gFieldMode ? "checked" : "") + ">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span style='font-size:13px; font-weight:bold; color:" + String(!gFieldMode ? "#eab308" : "var(--txt2)") + ";'>⚙️ Setup</span>";
  html += "</div></div>";

  // --- RÁCS KEZDŐDIK ---
  html += "<div class='dash-grid'>";

  // 1. NFC kártya
  html += "<div class='card'>";
  html += "<h2 style='font-size:14px; margin-bottom:8px;'>📱 NFC / RFID</h2>";
  html += "<div style='flex:1; display:flex; align-items:stretch;'>";
  html += "<button style='width:100%; min-height:100px; font-size:24px; font-weight:900; background:var(--accent); color:#fff; border:none; border-radius:12px; box-shadow:0 8px 16px rgba(77,77,255,0.3); text-transform:uppercase; letter-spacing:1px; cursor:pointer;' onclick=\"location.href='/nfc'\">📡 Olvasás</button>";
  html += "</div></div>";

  // 2. Hálózat & GNSS kártya
  String modemDot = gModem.ready ? "dot-g" : "dot-r";
  String gsmDot = gModem.registered ? "dot-g" : "dot-r";
  String netDot = gData.active ? "dot-g" : "dot-y";
  String opDot = gModem.registered ? "dot-g" : "dot-r";

  String gnssPwrDot = gGnss.enabled ? "dot-g" : "dot-r";
  String fixDot = "dot-r";
  String fixText = "Nincs";
  if (gGnss.fix) {
    if (gGnss.hdop < 2.5 && gGnss.satUsed >= 5) {
      fixDot = "dot-g"; fixText = "3D (" + String(gGnss.satUsed) + ")";
    } else {
      fixDot = "dot-y"; fixText = "2D (" + String(gGnss.satUsed) + ")";
    }
  }

  html += "<div class='card'>";
  html += "<h2 style='font-size:14px; margin-bottom:8px;'>📶 Hálózat & GNSS</h2>";
  html += "<div style='flex:1;'>";
  html += "<div class='compact-row'><span><span class='dot " + modemDot + "'></span>Modem</span><b>" + String(gModem.ready ? "Kész" : "Init") + "</b></div>";
  html += "<div class='compact-row'><span><span class='dot " + gsmDot + "'></span>GSM</span><b>" + String(gModem.registered ? "OK" : "Offline") + "</b></div>";
  html += "<div class='compact-row'><span><span class='dot " + netDot + "'></span>Adat</span><b>" + String(gData.active ? "Aktív" : "Inaktív") + "</b></div>";
  html += "<div class='compact-row'><span><span class='dot " + opDot + "'></span>Opr.</span><b>" + (gModem.registered ? gModem.operatorName : "-") + "</b></div>";
  html += "<hr style='border:0; border-top:1px solid rgba(255,255,255,0.1); margin:6px 0;'>";
  html += "<div class='compact-row'><span><span class='dot " + gnssPwrDot + "'></span>GNSS</span><b>" + String(gGnss.enabled ? "BE" : "KI") + "</b></div>";
  html += "<div class='compact-row'><span><span class='dot " + fixDot + "'></span>Fix</span><b>" + fixText + "</b></div>";
  html += "<div class='compact-row'><span>HDOP</span><b>" + String(gGnss.hdop, 1) + "</b></div>";
  html += "</div>";
  
  if(!gFieldMode) { 
    html += "<div style='display:flex; gap:6px; margin-top:8px;'>";
    html += "<a href='/gsm' style='flex:1;'><button class='sec' style='padding:6px; font-size:11px; width:100%;'>GSM</button></a>";
    html += "<a href='/gnss' style='flex:1;'><button class='sec' style='padding:6px; font-size:11px; width:100%;'>GNSS</button></a>";
    html += "</div>";
  }
  html += "</div>";

  // 3. Szenzorok kártya
  html += "<div class='card'>";
  html += "<h2 style='font-size:14px; margin-bottom:8px;'>🌡 Aktív Szenzorok</h2>";
  html += "<div style='flex:1;'>";

  int activeCount = 0;
  auto addSensRow = [&](String name, bool enabled, unsigned long lastRead) {
    if (!enabled) return;
    activeCount++;
    String dot = "dot-g";
    String val = "OK";
    if (lastRead == 0) { dot = "dot-y"; val = "Nincs adat"; }
    html += "<div class='compact-row'><span><span class='dot " + dot + "'></span>" + name + "</span><b>" + val + "</b></div>";
  };

  addSensRow("Belső Hő/Pára", gSht.enabled, gSht.lastGoodRead);
  addSensRow("Szélsebesség", gWindSpeed.enabled, gWindSpeed.lastGoodRead);
  addSensRow("Szélirány", gWindDir.enabled, gWindDir.lastGoodRead);
  addSensRow("Csapadék", gRain.enabled, gRain.lastPoll);
  addSensRow("Külső Hő/Nyomás", gAhtBmp.enabled, gAhtBmp.lastGoodRead);
  addSensRow("UV Index", gLtr.enabled, gLtr.lastGoodRead);
  addSensRow("Mérleg Dőlés", gMpu.enabled, gMpu.lastGoodRead);

  if (activeCount == 0) {
    html += "<p class='hint' style='margin:4px 0; font-size:12px;'>Nincs bekapcsolt szenzor.</p>";
  }

  html += "</div>";
  if(!gFieldMode) {
    html += "<a href='/sensors' style='margin-top:8px;'><button class='sec' style='padding:6px; font-size:11px; width:100%;'>Összes szenzor</button></a>";
  }
  html += "</div>";

  // 4. Időjárás kártya
  html += "<div class='card'>";
  html += "<h2 style='font-size:14px; margin-bottom:8px;'>🌤 Időjárás & Előrejelzés</h2>";
  html += "<div style='font-size:12px; margin-bottom:6px;'><b>Rendszeridő:</b> " + (gTime.synced ? gTime.localTime : "Nincs szinkron") + "</div>";
  html += "<hr style='border:0; border-top:1px solid var(--border); margin:6px 0;'>";

  html += "<div style='flex:1;'>";
  if(gWeatherHasData) {
    const char* dayNames[] = {"Ma", "Holnap", "Holnapután"};
    const char* timeSlots[] = {"00-06", "06-12", "12-18", "18-24"};
    
    html += "<div style='display:flex; flex-direction:column; gap:8px;'>";
    for(int d = 0; d < 3; d++) {
      html += "<div>";
      html += "<div style='font-weight:bold; color:var(--accent); font-size:12px; margin-bottom:4px;'>" + String(dayNames[d]) + "</div>";
      html += "<div style='display:grid; grid-template-columns: repeat(4, 1fr); gap:6px; text-align:center; font-size:11px;'>";
      for(int b = 0; b < 4; b++) {
        float minT = gForecast[d].blocks[b].tempMin;
        float maxT = gForecast[d].blocks[b].tempMax;
        float p = gForecast[d].blocks[b].precip;
        
        String icon = "☀️";
        if (p > 15.0) icon = "🧊";
        else if (p > 5.0) icon = "⚡";
        else if (p > 0.5) icon = "🌧️";
        else if ((minT + maxT) / 2.0 < 15) icon = "⛅";
        
        html += "<div style='background:rgba(255,255,255,0.04); padding:4px 2px; border-radius:6px; border:1px solid var(--border);'>"
                "<div style='font-size:9px; color:var(--txt2);'>" + String(timeSlots[b]) + "</div>"
                "<div style='font-size:14px; margin:2px 0;'>" + icon + "</div>"
                "<div style='font-size:10px; font-weight:bold;'>" + String(minT, 0) + " - " + String(maxT, 0) + "°C</div>"
                "</div>";
      }
      html += "</div></div>";
    }
    html += "</div>";
    html += "<p class='hint' style='margin-top:8px; margin-bottom:0; font-size:11px;'>Frissítve: " + ageText(gLastWeatherSync) + "</p>";
  } else {
    html += "<p class='hint' style='margin:0; font-size:12px;'>Nincs időjárás adat.</p>";
  }
  html += "</div></div>";

  // 5. Naptár
  html += getCalendarCardHtml();

  html += "</div>"; // dash-grid vége

  html += htmlFoot();
  server.send(200, "text/html", html);
}


// =================================================================================
// ÚTVONALAK REGISZTRÁLÁSA (SERVER.ON)
// =================================================================================
void webBegin() {
  
  // --- Alap UI és Rendszer ---
  server.on("/", handleRoot);
  server.on("/s.css", handleCss); 
  server.on("/cfg", handleCfg);
  server.on("/setmode", handleSetMode);
  server.on("/reinit", HTTP_POST, handleReinit);
  server.on("/esprestart", HTTP_POST, handleEspRestart);
  
  // --- Kaptárkezelés, Értékelés és Beavatkozások ---
  server.on("/hives", handleHives);
  server.on("/hive", handleHiveView);
  server.on("/treatment", handleTreatment);
  server.on("/evaluation", handleEvaluation);
  server.on("/api/evaluations", HTTP_GET, handleGetEvaluationsJson);
  server.on("/api/treatments", HTTP_GET, handleGetTreatmentsJson);
  server.on("/evaluate_post", HTTP_POST, handleEvaluatePost);
  server.on("/config", handleConfig);
  server.on("/config_post", HTTP_POST, handleConfigPost);
  server.on("/register_part", handleRegisterPart);
  server.on("/register_part_post", HTTP_POST, handleRegisterPartPost);
  server.on("/api/colony_functions", HTTP_GET, handleGetColonyFunctionsJson);
  server.on("/api/diseases", HTTP_GET, handleGetDiseasesJson);
  // --- Víztartály és szirupadagoló

  server.on("/supply", handleSupply);

  // --- NFC / RFID ---
  server.on("/nfc", handleNfc);

  // --- Modem és GSM funkciók ---
  server.on("/gsm", handleGsm);
  server.on("/modemstatus", handleModemStatus);
  server.on("/dosms", HTTP_POST, handleDoSms);
  server.on("/smsstatus", handleSmsStatus);
  server.on("/docall", HTTP_POST, handleDoCall);
  server.on("/hangup", HTTP_POST, handleHangup);
  server.on("/setsmsc", HTTP_POST, handleSetSmsc);
  server.on("/netauto", HTTP_POST, handleNetAuto);
  server.on("/netscan", HTTP_POST, handleNetScan);
  server.on("/netmanual", HTTP_POST, handleNetManual);

  // --- IoT, Adatkapcsolat és Mentések ---
  server.on("/iot", handleIot);
  server.on("/dataon", HTTP_POST, handleDataOn);
  server.on("/dataoff", HTTP_POST, handleDataOff);
  server.on("/dataping", HTTP_POST, handleDataPing);
  server.on("/ntfy-send", HTTP_POST, handleNtfySend);
  server.on("/ntfy-poll", HTTP_POST, handleNtfyPoll);
  server.on("/save-ntfy", HTTP_POST, handleSaveNtfy);
  server.on("/save-report", HTTP_POST, handleSaveReport);
  server.on("/test-report", HTTP_POST, handleTestReport);
  server.on("/eeprombackup", HTTP_POST, handleEepromBackup);
  server.on("/eepromrestore", HTTP_POST, handleEepromRestore);

  // --- GNSS és Helymeghatározás ---
  server.on("/gnss", handleGnss);
  server.on("/gnssstatus", handleGnssStatus);
  server.on("/gnssassist", HTTP_POST, handleGnssAssist);
  server.on("/gnssctl", HTTP_POST, handleGnssCtl);
  server.on("/api/map_status", HTTP_GET, handleMapStatusApi);

  // --- Szenzorok és Időjárás ---
  server.on("/sensors", handleSensors);
  server.on("/sensconfig", HTTP_POST, handleSensConfig);
  server.on("/senstoggle", HTTP_POST, handleSensToggle);
  server.on("/sensstatus", HTTP_GET, handleSensStatus);
  server.on("/senstest", HTTP_POST, handleSensTest);
  server.on("/saveweathercfg", HTTP_POST, handleSaveWeatherCfg);
  server.on("/testweatheralert", HTTP_POST, handleTestWeatherAlert);

  // --- Wi-Fi Beállítások ---
  server.on("/savewifi", HTTP_POST, handleSaveWifi);
  server.on("/wifiscan", HTTP_POST, handleWifiScan);
  server.on("/staconnect", HTTP_POST, handleStaConnect);
  server.on("/stadisconnect", HTTP_POST, handleStaDisconnect);

  // --- Diagnosztika és Haladó ---
  server.on("/diag", handleDiag);
  server.on("/expert", handleExpert);
  server.on("/at_ajax", handleAtAjax);
  server.on("/atstatus", HTTP_POST, handleAtStatus);
  server.on("/expertpost", HTTP_POST, handleExpertPost);
  server.on("/expertreset", HTTP_POST, handleExpertReset);
  server.on("/expertfullreset", HTTP_POST, handleExpertFullReset);
  server.on("/api/atstatus_serial", HTTP_GET, handleAtStatusSerial);
  server.on("/api/hives/list", HTTP_GET, handleGetHivesJson);
  server.on("/api/hives/delete", HTTP_POST, handleDeleteHive);
  server.on("/api/hives/add_dummy", HTTP_POST, handleAddDummyHive);
  server.on("/api/espnow_log", HTTP_GET, handleApiEspNowLog);
  server.on("/api/espnow_clear", HTTP_GET, handleApiEspNowClear);
  
  // --- Statikus fájlok kiszolgálása a LittleFS-ből ---

  server.on("/reg/start", handleRegStart);
  server.on("/reg/nfc", handleRegNfc);
  server.on("/reg/queen", HTTP_POST, handleRegQueen);
  server.on("/reg/survey", HTTP_POST, handleRegSurvey);
  server.on("/api/survey_status", HTTP_GET, handleApiSurveyStatus);
  server.on("/reg/summary", HTTP_GET, handleRegSummary);
  server.on("/reg/save", HTTP_POST, handleRegSave);
  server.on("/reg/cancel", HTTP_GET, handleRegCancel);

  server.serveStatic("/", LittleFS, "/");

  server.begin();
}