#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "calendar.h"
#include "config.h"
#include "gnss_mgr.h"
#include "modem_mgr.h"
#include "time_mgr.h"
#include "web_backup.h"
#include "web_common.h"
#include "web_config.h"
#include "web_gnss.h"
#include "web_gsm.h"
#include "web_iot.h"
#include "web_hives.h"
#include "web_sensors.h"
#include "web_diag.h"
#include "web_theme.h"
#include "web_ui.h"

extern WebServer server;
extern DNSServer dnsServer;
extern ModemState gModem;
extern GnssState gGnss;
extern TimeState gTime;

// Ha a handleCss a web_theme.cpp-ben van:
extern void handleCss(); 

void handleRoot() {
  if (!checkPinGuard()) return;
  String html = htmlHead("Áttekintés", "1");

  html += "<div class='card'><h2>Modem Állapot</h2>";
  html += stateRow("Modem kész", gModem.ready ? "Igen" : "Nem", gModem.ready ? "g" : "r");
  html += stateRow("Regisztrálva", gModem.registered ? "Igen" : "Nem", gModem.registered ? "g" : "r");
  html += stateRow("Operátor", gModem.operatorName.length() ? gModem.operatorName : "Ismeretlen");
  html += stateRow("Jelminőség", String(gModem.signalQuality));
  html += stateRow("Hálózati típus", gModem.netType.length() ? gModem.netType : "Ismeretlen");
  html += "</div>";

  html += "<div class='card'><h2>GPS / GNSS Pozíció</h2>";
  html += stateRow("GPS Fix", gGnss.fix ? "Van Fix" : "Nincs Fix", gGnss.fix ? "g" : "y");
  html += stateRow("Szélesség", String(gGnss.lat, 6));
  html += stateRow("Hosszúság", String(gGnss.lon, 6));
  html += stateRow("Műholdak száma", String(gGnss.satUsed));
  html += "</div>";

  html += "<div class='card'><h2>Idő & Rendszer</h2>";
  html += stateRow("Helyi idő", gTime.synced ? gTime.localTime : "Szinkronizálás alatt...", gTime.synced ? "g" : "y");
  html += stateRow("Szabad memória", String(ESP.getFreeHeap() / 1024) + " KB");
  html += stateRow("Uptime", String(millis() / 60000) + " perc");
  html += "</div>";

  html += getCalendarCardHtml();

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleJs() {
  if (!LittleFS.exists("/app.js")) {
    server.send(404, "text/plain", "app.js nem talalhato a LittleFS-en");
    return;
  }
  File f = LittleFS.open("/app.js", "r");
  server.streamFile(f, "application/javascript");
  f.close();
}

void handleStyle() {
  if (!LittleFS.exists("/style.css")) {
    server.send(404, "text/plain", "style.css nem talalhato a LittleFS-en");
    return;
  }
  File f = LittleFS.open("/style.css", "r");
  server.streamFile(f, "text/css");
  f.close();
}

void handleHomeApi() {
  String json = "{";
  json += "\"pinSaved\":" + String(loadPin().length() > 0 ? "true" : "false") + ",";
  json += "\"modemReady\":" + String(gModem.ready ? "true" : "false") + ",";
  json += "\"registered\":" + String(gModem.registered ? "true" : "false") + ",";
  json += "\"operator\":\"" + jsEscape(gModem.operatorName) + "\",";
  json += "\"signal\":" + String(gModem.signalQuality) + ",";
  json += "\"netType\":\"" + gModem.netType + "\",";
  json += "\"gnssFix\":" + String(gGnss.fix ? "true" : "false") + ",";
  json += "\"lat\":" + String(gGnss.lat, 6) + ",";
  json += "\"lon\":" + String(gGnss.lon, 6) + ",";
  json += "\"sat\":" + String(gGnss.satUsed) + ",";
  json += "\"time\":\"" + gTime.localTime + "\"";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleNotFound() {
  if(server.uri() == "/s.css") { handleCss(); return; }
  server.sendHeader("Location","http://192.168.4.1/",true);
  server.send(302,"text/plain","");
}

void webBegin() {
  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/app.js",     HTTP_GET,  handleJs);
  server.on("/style.css",  HTTP_GET,  handleStyle);
  server.on("/s.css",      HTTP_GET,  handleCss);
  server.on("/api/home",   HTTP_GET,  handleHomeApi);
  server.on("/ntfy-send",  HTTP_POST, handleNtfySend);
  server.on("/ntfy-poll",  HTTP_POST, handleNtfyPoll);
  server.on("/save-ntfy",  HTTP_POST, handleSaveNtfy);
  server.on("/gsm",        HTTP_GET,  handleGsm);
  server.on("/iot",        HTTP_GET,  handleIot);
  
  server.on("/dataon",     HTTP_POST, handleDataOn);
  server.on("/dataoff",    HTTP_POST, handleDataOff);
  server.on("/dataping",   HTTP_POST, handleDataPing);
  
  server.on("/gnss",       HTTP_GET,  handleGnss);
  server.on("/gnssstatus", HTTP_GET,  handleGnssStatus);
  server.on("/gnssctl",    HTTP_POST, handleGnssCtl);
  server.on("/gnssassist", HTTP_POST, handleGnssAssist);
  
  server.on("/sensors",    HTTP_GET,  handleSensors);
  server.on("/sensconfig", HTTP_POST, handleSensConfig);
  server.on("/senstoggle", HTTP_POST, handleSensToggle);
  server.on("/sensstatus", HTTP_GET,  handleSensStatus);
  server.on("/senstest",   HTTP_POST, handleSensTest);
  
  server.on("/expert",     HTTP_GET,  handleExpert);
  server.on("/expertpost", HTTP_POST, handleExpertPost);
  server.on("/expertreset",HTTP_POST, handleExpertReset);
  server.on("/expertfullreset", HTTP_POST, handleExpertFullReset);

  server.on("/cfg",        HTTP_GET,  handleCfg);
  server.on("/savewifi",   HTTP_POST, handleSaveWifi);
  server.on("/wifiscan",   HTTP_POST, handleWifiScan);
  server.on("/staconnect", HTTP_POST, handleStaConnect);
  server.on("/stadisconnect", HTTP_POST, handleStaDisconnect);
  server.on("/savepin",    HTTP_POST, handleSavePin);
  server.on("/changepin",  HTTP_POST, handleChangePin);
  server.on("/testsavepin",HTTP_POST, handleTestSavePin);
  server.on("/confirmsavepin",HTTP_POST, handleConfirmSavePin);
  server.on("/savepanelver",HTTP_POST, handleSavePanelVer);
  server.on("/ledtrigger", HTTP_POST, handleLedTrigger);
  server.on("/ledauto",    HTTP_POST, handleLedAuto);
  
  server.on("/diag",       HTTP_GET,  handleDiag);
  server.on("/at_ajax",    HTTP_GET,  handleAtAjax);
  server.on("/atstatus",   HTTP_POST, handleAtStatus);
  server.on("/eeprombackup",HTTP_GET, handleEepromBackup);
  server.on("/eepromrestore", HTTP_POST, handleEepromRestore);
  server.on("/setsmsc",    HTTP_POST, handleSetSmsc);
  server.on("/dosms",      HTTP_POST, handleDoSms);
  server.on("/smsstatus",  HTTP_GET,  handleSmsStatus);
  server.on("/docall",     HTTP_POST, handleDoCall);
  server.on("/hangup",     HTTP_POST, handleHangup);
  server.on("/modemstatus",HTTP_GET,  handleModemStatus);
  server.on("/reinit",     HTTP_POST, handleReinit);

  server.on("/hives",      HTTP_GET,  handleHives);
  server.on("/test-report",HTTP_POST, handleTestReport);
  server.on("/netauto",    HTTP_POST, handleNetAuto);
  server.on("/netscan",    HTTP_POST, handleNetScan);
  server.on("/netmanual",  HTTP_POST, handleNetManual);

  server.on("/esprestart", HTTP_POST, handleEspRestart);

  server.on("/save-report",HTTP_POST, handleSaveReport);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("[WEB] Webszerver elindult."));
}