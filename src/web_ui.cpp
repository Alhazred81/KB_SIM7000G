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
extern void handleNfc();

// --- Külső modulokból származó handler függvények deklarációi ---

extern void handleHives();
extern void handleCfg();
extern void handleGsm();
extern void handleIot();
extern void handleGnss();
extern void handleSensors();
extern void handleDiag();
extern void handleExpert();
extern void handleHiveView();

extern void handleDoSms();
extern void handleSmsStatus();
extern void handleDoCall();
extern void handleHangup();
extern void handleSetSmsc();
extern void handleNetAuto();
extern void handleNetScan();
extern void handleNetManual();

extern void handleDataOn();
extern void handleDataOff();
extern void handleDataPing();
extern void handleNtfySend();
extern void handleNtfyPoll();
extern void handleSaveReport();
extern void handleTestReport();

extern void handleEepromBackup();
extern void handleEepromRestore();

extern void handleSensConfig();
extern void handleSensToggle();
extern void handleSensStatus();
extern void handleSensTest();

extern void handleAtAjax();
extern void handleAtStatus();
extern void handleModemStatus();
extern void handleReinit();
extern void handleExpertPost();
extern void handleExpertReset();
extern void handleExpertFullReset();
extern void handleEspRestart();

extern void handleGnssStatus();
extern void handleGnssAssist();
extern void handleGnssCtl();

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

  // Brutális CSS felülírás: Szabadítsuk ki a kártyákat a szűk konténerből!
  html += "<style>"
          "main, .container, #content { max-width: 100% !important; width: 100% !important; padding: 15px !important; box-sizing: border-box !important; }" 
          ".dot { height: 10px; width: 10px; border-radius: 50%; display: inline-block; margin-right: 6px; vertical-align: middle; }"
          ".dot-g { background-color: #22c55e; box-shadow: 0 0 5px rgba(34,197,94,0.6); }"
          ".dot-y { background-color: #eab308; box-shadow: 0 0 5px rgba(234,179,8,0.6); }"
          ".dot-r { background-color: #ef4444; box-shadow: 0 0 5px rgba(239,68,68,0.6); }"
          ".compact-row { display: flex; align-items: center; justify-content: space-between; padding: 4px 0; border-bottom: 1px solid rgba(255,255,255,0.04); font-size: 13px; }"
          
          /* Flexbox a grid helyett: garantáltan szétterül, ha van hely! */
          ".dash-grid { display: flex; flex-wrap: wrap; gap: 16px; width: 100%; align-items: stretch; justify-content: flex-start; }"
          ".dash-grid > .card, .dash-grid > div.card { flex: 1 1 300px; min-width: 260px; max-width: none !important; margin: 0 !important; box-sizing: border-box; display: flex; flex-direction: column; }"
          "</style>";

  html += "<div class='dash-grid'>";

  // 1. NFC kártya
  html += "<div class='card' style='padding:12px;'>";
  html += "<h2 style='font-size:15px; margin-bottom:8px;'>📱 NFC / RFID</h2>";
  html += "<div style='flex:1; display:flex; align-items:center;'>";
  html += "<button class='sec' style='width:100%; padding:10px; font-size:14px;' onclick=\"location.href='/nfc'\">📡 Olvasás</button>";
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
      fixDot = "dot-g"; fixText = "3D Fix (" + String(gGnss.satUsed) + ")";
    } else {
      fixDot = "dot-y"; fixText = "2D Fix (" + String(gGnss.satUsed) + ")";
    }
  }

  html += "<div class='card' style='padding:12px;'>";
  html += "<h2 style='font-size:15px; margin-bottom:8px;'>📶 Hálózat & GNSS</h2>";
  html += "<div style='flex:1;'>";
  html += "<div class='compact-row'><span><span class='dot " + modemDot + "'></span>Modem</span><b>" + String(gModem.ready ? "Kész" : "Init") + "</b></div>";
  html += "<div class='compact-row'><span><span class='dot " + gsmDot + "'></span>GSM Hálózat</span><b>" + String(gModem.registered ? "OK" : "Offline") + "</b></div>";
  html += "<div class='compact-row'><span><span class='dot " + netDot + "'></span>Internet (Adat)</span><b>" + String(gData.active ? "Aktív" : "Inaktív") + "</b></div>";
  html += "<div class='compact-row'><span><span class='dot " + opDot + "'></span>Szolgáltató</span><b>" + (gModem.registered ? gModem.operatorName : "-") + "</b></div>";
  html += "<hr style='border:0; border-top:1px solid rgba(255,255,255,0.1); margin:8px 0;'>";
  html += "<div class='compact-row'><span><span class='dot " + gnssPwrDot + "'></span>GNSS Vevő</span><b>" + String(gGnss.enabled ? "BE" : "KI") + "</b></div>";
  html += "<div class='compact-row'><span><span class='dot " + fixDot + "'></span>Műholdas Fix</span><b>" + fixText + "</b></div>";
  html += "<div class='compact-row'><span>HDOP Pontosság</span><b>" + String(gGnss.hdop, 1) + "</b></div>";
  html += "</div>";
  html += "<div style='display:flex; gap:6px; margin-top:8px;'>";
  html += "<a href='/gsm' style='flex:1;'><button class='sec' style='padding:6px; font-size:12px; width:100%;'>GSM</button></a>";
  html += "<a href='/gnss' style='flex:1;'><button class='sec' style='padding:6px; font-size:12px; width:100%;'>GNSS</button></a>";
  html += "</div>";
  html += "</div>";

  // 3. Szenzorok kártya
  html += "<div class='card' style='padding:12px;'>";
  html += "<h2 style='font-size:15px; margin-bottom:8px;'>🌡 Bekapcsolt Szenzorok</h2>";
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
    html += "<p class='hint' style='margin:4px 0;'>Nincs bekapcsolt szenzor.</p>";
  }

  html += "</div>";
  html += "<a href='/sensors' style='margin-top:8px;'><button class='sec' style='padding:6px; font-size:12px; width:100%;'>Összes szenzor</button></a>";
  html += "</div>";

  // 4. Időjárás kártya
  html += "<div class='card' style='padding:12px;'>";
  html += "<h2 style='font-size:15px; margin-bottom:8px;'>🌤 Időjárás & Előrejelzés</h2>";
  html += "<div style='font-size:13px; margin-bottom:6px;'><b>Rendszeridő:</b> " + (gTime.synced ? gTime.localTime : "Nincs szinkron") + "</div>";
  html += "<hr style='border:0; border-top:1px solid var(--border); margin:8px 0;'>";

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
        float t = (gForecast[d].blocks[b].tempMin + gForecast[d].blocks[b].tempMax) / 2.0;
        float p = gForecast[d].blocks[b].precip;
        
        String icon = "☀️";
        if (p > 15.0) icon = "🧊";
        else if (p > 5.0) icon = "⚡";
        else if (p > 0.5) icon = "🌧️";
        else if (t < 15) icon = "⛅";
        
        html += "<div style='background:rgba(255,255,255,0.04); padding:4px; border-radius:6px; border:1px solid var(--border);'>"
                "<div style='font-size:10px; color:var(--txt2);'>" + String(timeSlots[b]) + "</div>"
                "<div style='font-size:14px; margin:2px 0;'>" + icon + "</div>"
                "<div>" + String(t, 0) + "°C</div>"
                "</div>";
      }
      html += "</div></div>";
    }
    html += "</div>";
    html += "<p class='hint' style='margin-top:8px; margin-bottom:0;'>Frissítve: " + ageText(gLastWeatherSync) + "</p>";
  } else {
    html += "<p class='hint' style='margin:0;'>Nincs időjárás adat.</p>";
  }
  html += "</div></div>";

  // 5. Naptár
  html += getCalendarCardHtml();

  html += "</div>"; // dash-grid vége

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
  server.on("/eeprombackup", HTTP_POST, handleEepromBackup);
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
  server.on("/expertreset", HTTP_POST, handleExpertReset);
  server.on("/expertfullreset", HTTP_POST, handleExpertFullReset);
  server.on("/esprestart", HTTP_POST, handleEspRestart);

  // GNSS végpontok
  server.on("/gnssstatus", handleGnssStatus);
  server.on("/gnssassist", HTTP_POST, handleGnssAssist);
  server.on("/gnssctl", HTTP_POST, handleGnssCtl);

  // Egyéb funkciók
  server.on("/nfc", handleNfc);
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

  server.serveStatic("/", LittleFS, "/");

  server.begin();
}