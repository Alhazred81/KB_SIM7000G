//web_ui.cpp

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "calendar.h"
#include "config.h"
#include "gnss_mgr.h"
#include "modem_mgr.h"
#include "NtfyClient.h"
#include "sensors.h"
#include "time_mgr.h"
#include "wifi_sta.h"
#include "web_backup.h"
#include "web_common.h"
#include "web_gsm.h"
#include "web_iot.h"
#include "web_sensors.h"
#include "web_theme.h"
#include "web_ui.h"


extern WebServer server;
extern DNSServer dnsServer;
extern ModemState gModem;
extern GnssState gGnss;
extern TimeState gTime;
extern WifiStaState gSta;
extern DataConnState gData;
extern LedConfig gLed;
extern NtfyClient ntfy;
extern String gNtfyServer;
extern String gNtfyTopic;
extern String gNtfyNickname;
extern bool gNtfyStartupMsg;
extern WindSpeedState gWindSpeed;
extern WindDirState gWindDir;
extern ShtSensorState gSht;
extern RainSensorState gRain;
extern Mpu6050State gMpu;
extern Aht20Bmp280State gAhtBmp;
extern Ltr390State gLtr;

extern String gApSSID;
extern String gApPass;
extern uint8_t gApChannel;
extern unsigned long gLastSms;
extern bool gDiagEnabled;
extern bool gModemInitRequested;

extern bool gSmsSendRequested;
extern String gSmsPendingNum;
extern String gSmsPendingText;
extern bool gSmsSendInProgress;
extern bool gSmsSendDone;
extern String gSmsSendResult;
extern bool checkPinGuard();

extern ScannedNet gScanResults[];
extern int gScanCount;

extern void saveNtfyConfig(const String& server, const String& topic, const String& nickname, bool startupMsg);
extern String macSuffix();

// Ideiglenes memória a sikeresen tesztelt, de még nem mentett PIN-nek
static String gLastValidPin = "";

String satRow(const String& systemName, int count) {
  return stateRow(systemName, satText(count));
}

void sendWaitPage(const String& title, const String& message, const String& nextUrl, int waitSeconds) {
  String html = htmlHead(title, "");
  html += "<style>@keyframes spin { 100% { transform: rotate(360deg); } }</style>";
  html += "<div style='display:flex; justify-content:center; padding-top:40px;'>";
  html += "<div class='card' style='text-align:center; padding:40px 20px; max-width:400px; width:100%;'>";
  html += "<h2 style='font-size:18px; margin-bottom:15px;'>" + title + "</h2>";
  html += "<div style='font-size:40px; margin:20px 0; display:inline-block; animation:spin 3s linear infinite;'>⚙️</div>";
  html += "<p style='font-size:14px; color:var(--txt); margin-bottom:20px;'>" + message + "</p>";
  html += "<div class='msg warn' id='countdown' style='font-size:14px; font-weight:bold;'>Hátravan max: " + String(waitSeconds) + " mp</div>";
  html += "</div></div>";
  html += "<script>";
  
  html += "var w = " + String(waitSeconds) + ";";
  html += "var t = setInterval(function(){ "
          "  w--; "
          "  if(w > 0) document.getElementById('countdown').innerText = 'Hátravan max: ' + w + ' mp'; "
          "  else location.href='" + nextUrl + "'; "
          "}, 1000);";

  html += "var p = setInterval(function(){"
          "  fetch('/modemstatus').then(function(r){return r.json();}).then(function(d){"
          "    if(d && d.inProgress === false) { "
          "      clearInterval(t); clearInterval(p);"
          "      document.getElementById('countdown').innerText = 'Kész! Átirányítás...';"
          "      document.getElementById('countdown').className = 'msg ok';"
          "      setTimeout(function(){ location.href='" + nextUrl + "'; }, 500);"
          "    }"
          "  }).catch(function(){});"
          "}, 3000);";
          
  html += "</script>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

static String windSpeedValueText() {
  if(!gWindSpeed.enabled) return "";
  if(!gWindSpeed.lastReadOk && gWindSpeed.lastGoodRead == 0) return "";
  return String(gWindSpeed.speedMs, 1) + " m/s";
}

static String windDirValueText() {
  if(!gWindDir.enabled) return "";
  if(!gWindDir.lastReadOk && gWindDir.lastGoodRead == 0) return "";
  return String(gWindDir.directionDeg, 0) + "\xC2\xB0 " + compassAbbrev(gWindDir.directionDeg);
}

static String shtValueText() {
  if(!gSht.enabled) return "";
  if(!gSht.lastReadOk && gSht.lastGoodRead == 0) return "";
  return String(gSht.tempC, 1) + " C, " + String(gSht.humidityPct, 0) + "%";
}

static String rainValueText() {
  if(!gRain.enabled) return "";
  return String(gRain.percentWet) + "% " + (gRain.isRaining ? "(esik)" : "(szaraz)");
}

static String mpuValueText() {
  if(!gMpu.enabled) return "";
  if(!gMpu.lastReadOk && gMpu.lastGoodRead == 0) return "";
  return String(gMpu.accelX,2)+","+String(gMpu.accelY,2)+","+String(gMpu.accelZ,2)+" g";
}

static String ahtBmpValueText() {
  if(!gAhtBmp.enabled) return "";
  if(!gAhtBmp.lastReadOk && gAhtBmp.lastGoodRead == 0) return "";
  String s = "";
  if(gAhtBmp.ahtOk) s += String(gAhtBmp.ahtTempC,1) + "C " + String(gAhtBmp.ahtHumidityPct,0) + "%";
  if(gAhtBmp.bmpOk) { if(s.length()) s += " | "; s += String(gAhtBmp.bmpPressureHpa,0) + "hPa"; }
  return s;
}

static String ltrValueText() {
  if(!gLtr.enabled) return "";
  if(!gLtr.lastReadOk && gLtr.lastGoodRead == 0) return "";
  return "UVI " + String(gLtr.uvIndex, 1);
}

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

void handleEspRestart() {
  String html = htmlHead("Rendszer Újraindítás", "5");
  html += "<div class='card' style='text-align:center; padding:30px;'>";
  html += "<h2>Az ESP32 újraindul...</h2>";
  html += "<p class='hint'>A kapcsolat megszakad, kérlek várj pár másodpercet, majd frissítsd az oldalt.</p>";
  html += "</div>";
  html += htmlFoot();
  server.send(200, "text/html", html);
  
  diagAdd("ESP32 kézi újraindítás webes felületről.");
  delay(1000); 
  ESP.restart();
}


void handleGnss() {
  if (!checkPinGuard()) return;
  String html = htmlHead("GPS", "6");

  if(!gModem.ready){
    html += "<div class='msg err'>A modem nincs aktiv, GNSS nem indithato.</div>";
  }

  html += "<div class='card'><h2>Vevo allapot</h2>";
  html += stateRow("Vevo", gnssReceiverStatusText(), gGnss.fix ? "g" : (gGnss.enabled ? "y" : "r"));
  html += stateRow("GNSS kapcsolo", gGnss.enabled ? "BE" : "KI", gGnss.enabled ? "g" : "r");
  html += stateRow("Run status", String(gGnss.runStatus));
  html += stateRow("Fix status", String(gGnss.fixStatus));
  html += stateRow("Utolso GNSS poll", ageText(gGnss.lastPoll));
  html += stateRow("Pozicio poll", ageText(gGnss.lastPositionPoll));
  html += stateRow("Muheld poll", ageText(gGnss.lastExtendedPoll));
  html += stateRow("Antenna poll", ageText(gGnss.lastAntennaPoll));
  html += stateRow("Inditas ota", gGnss.startedAt ? ageText(gGnss.startedAt) : "meg nem indult");
  html += stateRow("Utolsó fix", gGnss.lastGoodFix ? ageText(gGnss.lastGoodFix) : "meg nem volt");
  if(gGnss.lastError.length()) html += stateRow("Utolsó hiba", htmlEscape(gGnss.lastError), "y");
  html += stateRow("NTP ido", gTime.synced ? gTime.localTime : "nincs szinkron", gTime.synced ? "g" : "y");
  html += stateRow("GNSS UTC", gGnss.dateStr + " " + gGnss.timeStr);
  html += "</div>";

  html += "<div class='card wide'><h2>Kiindulo koordinata</h2>";
  html += stateRow("Latitude", String(gGnss.assistLat, 6));
  html += stateRow("Longitude", String(gGnss.assistLon, 6));
  html += "<form action='/gnssassist' method='POST'>";
  html += "<label>Latitude</label><input type='text' name='lat' value='" + String(gGnss.assistLat, 6) + "' inputmode='decimal'>";
  html += "<label>Longitude</label><input type='text' name='lon' value='" + String(gGnss.assistLon, 6) + "' inputmode='decimal'>";
  html += "<button class='sec'>Koordinata mentese</button></form>";
  html += "</div>";

  html += "<div class='card diag-card wide'><h2>🛰️ GNSS Live Debug</h2>";
  html += "<div class='diag' id='gnssDebugBox' style='max-height:200px; overflow-y:auto; font-size:11px;'>Betöltés...</div>";
  html += "<script>";
  html += "function pollGnssDebug(){";
  html += "  fetch('/gnssstatus').then(function(r){return r.json();}).then(function(d){";
  html += "    var txt = 'Engedélyezve: ' + (d.enabled ? 'BE' : 'KI') + '\\n';";
  html += "    txt += 'Fix: ' + (d.fix ? 'VAN' : 'NINCS') + '\\n';";
  html += "    txt += 'Használt műholdak: ' + d.satUsed + '\\n';";
  html += "    txt += 'Látható holdak: ' + d.satView + '\\n';";
  html += "    txt += 'HDOP: ' + d.hdop + '\\n';";
  html += "    txt += 'Lat/Lon: ' + d.lat + ', ' + d.lon + '\\n';";
  html += "    document.getElementById('gnssDebugBox').innerText = txt;";
  html += "  }).catch(function(){});";
  html += "}";
  html += "setInterval(pollGnssDebug, 2000);";
  html += "pollGnssDebug();";
  html += "</script>";
  html += "</div>";

  if(!gGnss.enabled){
    html += "<div class='card'><h2>GNSS kikapcsolva</h2>"
            "<form action='/gnssctl' method='POST'>"
            "<input type='hidden' name='action' value='start'>"
            "<button>🛰 GNSS bekapcsolasa</button>"
            "</form></div>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  double activeLat = gGnss.fix ? gGnss.lat : gGnss.assistLat;
  double activeLon = gGnss.fix ? gGnss.lon : gGnss.assistLon;
  char latS[16], lonS[16];
  dtostrf(activeLat, 0, 6, latS);
  dtostrf(activeLon, 0, 6, lonS);

  html += "<div class='card'><h2>Pozicio";
  if(gGnss.fix) html += " <span style='color:var(--ok);font-size:11px'>● FIX</span>";
  else          html += " <span style='color:var(--warn);font-size:11px'>● Nincs fix</span>";
  html += "</h2>";

  html += stateRow("Szelesseg", String(latS)+"°");
  html += stateRow("Hosszusag", String(lonS)+"°");
  html += stateRow("Magassag", String(gGnss.alt,1)+" m");
  html += stateRow("Sebesseg", String(gGnss.speed,1)+" km/h");
  html += stateRow("Irany", String(gGnss.course,1)+"°");
  html += stateRow("HDOP", String(gGnss.hdop,1));

  html += "<div class='card wide' style='grid-column:1/-1'>"
          "<h2>Térkép</h2>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<div id='map' style='height:260px;border-radius:8px;margin-top:6px;z-index:1'></div>"
          "<script>"
          "var osmLayer = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {maxZoom: 19, attribution: '© OpenStreetMap'});"
          "var satLayer = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {maxZoom: 19, attribution: 'Tiles &copy; Esri'});"
          
          "var map = L.map('map', {center: [" + String(latS) + ", " + String(lonS) + "], zoom: 15, layers: [osmLayer]});"
          
          "var baseLayers = {'Utca': osmLayer, 'Műhold': satLayer};"
          "L.control.layers(baseLayers).addTo(map);"

          "var marker = L.marker([" + String(latS) + ", " + String(lonS) + "]).addTo(map)"
            ".bindPopup('" + String(gGnss.fix ? "Aktuális fix" : "Kiinduló hely") + "').openPopup();"
          "var lastLat = " + String(latS) + ", lastLon = " + String(lonS) + ", lastFix = " + String(gGnss.fix ? "true" : "false") + ";"
          "function updateGnssMap(){"
            "fetch('/gnssstatus').then(function(r){return r.json();}).then(function(d){"
              "if(d.fix && (!lastFix || d.lat !== lastLat || d.lon !== lastLon)){"
                "marker.setLatLng([d.lat, d.lon]);"
                "map.setView([d.lat, d.lon], 16);"
                "marker.bindPopup('Aktuális fix').openPopup();"
                "lastLat = d.lat; lastLon = d.lon; lastFix = d.fix;"
              "}"
            "}).catch(function(){});"
          "}"
          "setInterval(updateGnssMap, 5000);"
          "</script>"
          "</div>";

  char mapUrl[96];
  snprintf(mapUrl, sizeof(mapUrl), "https://maps.google.com/?q=%s,%s", latS, lonS);
  html += "<a href='" + String(mapUrl) + "' target='_blank'><button class='sec' style='margin-top:10px'>🗺 Megnyitas Google Maps-en</button></a>";
  html += "</div>";

  html += "<div class='card'><h2>Műholdak</h2>";
  html += stateRow("Osszes hasznalt", String(gGnss.satUsed));
  html += stateRow("GPS lathato", satText(gGnss.satGpsInView));
  html += satRow("GPS", gGnss.satGPS);
  html += satRow("GLONASS", gGnss.satGLO);
  html += satRow("BeiDou", gGnss.satBDS);
  html += satRow("Galileo", gGnss.satGAL);
  html += "</div>";

  html += "<div class='card diag-card'><h2>Nyers GNSS valaszok</h2><div class='diag'>";
  html += "CGNSINF: " + htmlEscape(gGnss.rawCgnsinf) + "\n";
  html += "CGNSSINFO: " + htmlEscape(gGnss.rawCgnssinfo);
  html += "</div></div>";

  html += "<form action='/gnssctl' method='POST'>"
          "<input type='hidden' name='action' value='stop'>"
          "<button class='sec'>GNSS kikapcsolasa</button></form>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleGnssAssist() {
  if(!server.hasArg("lat") || !server.hasArg("lon")) {
    server.sendHeader("Location","/gnss"); server.send(302); return;
  }
  float lat = server.arg("lat").toFloat();
  float lon = server.arg("lon").toFloat();
  if(lat < -90 || lat > 90 || lon < -180 || lon > 180) {
    diagAdd("GNSS koordinata HIBA: ervenytelen tartomany");
  } else {
    gnssSaveAssist(lat, lon);
    diagAdd("GNSS kiindulo koordinata mentve: " + String(lat, 6) + ", " + String(lon, 6));
  }
  server.sendHeader("Location","/gnss");
  server.send(302);
}

void handleGnssCtl() {
  if(sendModemBusyPage("GNSS", "6", "/gnss")) return;
  String action = server.hasArg("action") ? server.arg("action") : "";
  if(action == "start") gnssStart();
  else if(action == "stop") gnssStop();
  server.sendHeader("Location","/gnss");
  server.send(302);
}

void handleGnssStatus() {
  String json = "{";
  json += "\"enabled\":" + String(gGnss.enabled ? "true" : "false") + ",";
  json += "\"fix\":" + String(gGnss.fix ? "true" : "false") + ",";
  json += "\"lat\":" + String(gGnss.fix ? gGnss.lat : gGnss.assistLat, 6) + ",";
  json += "\"lon\":" + String(gGnss.fix ? gGnss.lon : gGnss.assistLon, 6) + ",";
  json += "\"alt\":" + String(gGnss.alt, 1) + ",";
  json += "\"speed\":" + String(gGnss.speed, 1) + ",";
  json += "\"course\":" + String(gGnss.course, 1) + ",";
  json += "\"hdop\":" + String(gGnss.hdop, 1) + ",";
  json += "\"satUsed\":" + String(gGnss.satUsed) + ",";
  json += "\"satView\":" + String(gGnss.satGpsInView);
  json += "}";
  server.send(200, "application/json", json);
}

void handleHives() {
  if (!checkPinGuard()) return;
  String html = htmlHead("Kaptárak", "9");

  html += "<style>"
          "@keyframes flammenwerfer { 0% { opacity: 1; background-color: rgba(255,0,0,0.3); } 50% { opacity: 0.4; background-color: rgba(255,0,0,0.8); } 100% { opacity: 1; background-color: rgba(255,0,0,0.3); } }"
          "@keyframes mapIconPulse { 0% { transform: scale(1); filter: drop-shadow(0 0 2px rgba(255,0,0,0.8)); } 50% { transform: scale(1.25); filter: drop-shadow(0 0 12px rgba(255,0,0,1)); } 100% { transform: scale(1); filter: drop-shadow(0 0 2px rgba(255,0,0,0.8)); } }"
          ".hive-pulse { animation: mapIconPulse 0.8s infinite ease-in-out; transform-origin: center; }"
          ".hive-table-container { max-height: 450px; overflow-y: auto; border: 1px solid var(--border); border-radius: 8px; }"
          "table.hive-table { width: 100%; border-collapse: collapse; text-align: left; font-size: 13px; }"
          "table.hive-table th { position: sticky; top: 0; background: var(--nav); color: var(--txt); padding: 10px; border-bottom: 2px solid var(--border); z-index: 2; }"
          "table.hive-table td { padding: 10px; border-bottom: 1px solid var(--border); }"
          ".badge { padding: 4px 8px; border-radius: 4px; font-weight: bold; font-size: 11px; display: inline-block; }"
          ".b-ok { background: rgba(0,204,102,0.2); color: var(--ok); }"
          ".b-yell { background: rgba(255,255,0,0.2); color: #e6e600; }"
          ".b-org { background: rgba(255,165,0,0.2); color: #ffa500; }"
          ".b-red { background: rgba(255,0,0,0.2); color: #ff3333; }"
          ".b-cyc { background: rgba(255,0,255,0.2); color: #ff00ff; }"
          ".b-flame { background: rgba(255,0,0,0.5); color: #fff; animation: flammenwerfer 0.8s infinite; }"
          ".dim { color: var(--txt2); font-size: 12px; }"
          ".custom-hive-icon { background: transparent; border: none; }"
          "</style>";

  html += "<div class='card wide' style='grid-column:1/-1'>"
          "<h2>🗺 Kaptárak Térképes Áttekintése</h2>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<div id='hiveMap' style='height:350px;border-radius:8px;margin-top:6px;z-index:1'></div>"
          "<script>"
          "var osmLayerH = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {maxZoom: 19, attribution: '© OpenStreetMap'});"
          "var satLayerH = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {maxZoom: 19, attribution: 'Tiles &copy; Esri'});"
          
          "var hiveMap = L.map('hiveMap', {center: [47.514600, 19.043500], zoom: 15, layers: [osmLayerH]});"
          "var baseLayersH = {'Utca': osmLayerH, 'Műhold': satLayerH};"
          "L.control.layers(baseLayersH).addTo(hiveMap);"

          "function getQueenColor(year) {"
          "  var lastDigit = year % 10;"
          "  if (lastDigit === 1 || lastDigit === 6) return '#FFFFFF';"
          "  if (lastDigit === 2 || lastDigit === 7) return '#FFFF00';"
          "  if (lastDigit === 3 || lastDigit === 8) return '#FF0000';"
          "  if (lastDigit === 4 || lastDigit === 9) return '#00FF00';"
          "  return '#0000FF';"
          "}"

          "function getInterventionColor(days) {"
          "  if (days === 'hans') return '#FF0000';"
          "  if (days > 3) return '#FFFF00';"
          "  if (days > 1) return '#FFA500';"
          "  if (days === 1) return '#FF0000';"
          "  if (days === 0) return '#FF00FF';"
          "  return '#00CC66';"
          "}"

          "function createSquareHiveIcon(days, queenYear, isCritical, hasSensorErr, batPct) {"
          "  var qColor = getQueenColor(queenYear);"
          "  var statusColor = getInterventionColor(days);"
          "  var pulseClass = isCritical ? ' hive-pulse' : '';"
          
          "  var errSvg = hasSensorErr ? '<circle cx=\"24\" cy=\"24\" r=\"9\" fill=\"#FF0000\" stroke=\"#2a2a40\" stroke-width=\"2\"/><text x=\"24\" y=\"28\" fill=\"white\" font-size=\"11\" font-family=\"sans-serif\" font-weight=\"bold\" text-anchor=\"middle\">!</text>' : '';"
          
          "  var batWidth = (batPct / 100) * 20;"
          "  var batColor = batPct > 20 ? '#00FF00' : '#FF0000';"
          "  var batSvg = '<rect x=\"40\" y=\"68\" width=\"20\" height=\"8\" fill=\"#333\" stroke=\"#2a2a40\" stroke-width=\"1.5\" rx=\"1\"/><rect x=\"40\" y=\"68\" width=\"' + batWidth + '\" height=\"8\" fill=\"' + batColor + '\" rx=\"1\"/><rect x=\"60\" y=\"70\" width=\"2\" height=\"4\" fill=\"#2a2a40\"/>';"

          "  var svg = '<svg class=\"' + pulseClass.trim() + '\" viewBox=\"0 0 100 100\" xmlns=\"http://www.w3.org/2000/svg\">' +"
          "    '<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" fill=\"' + statusColor + '\" stroke=\"#2a2a40\" stroke-width=\"6\" rx=\"12\"/>' +"
          "    '<circle cx=\"50\" cy=\"44\" r=\"14\" fill=\"' + qColor + '\" stroke=\"#2a2a40\" stroke-width=\"3\"/>' +"
          "    errSvg + batSvg +"
          "  '</svg>';"
          "  return L.divIcon({ className: 'custom-hive-icon', html: svg, iconSize: [36, 36], iconAnchor: [18, 18], popupAnchor: [0, -18] });"
          "}"

          "var markers = [];"
          
          "var hivesData = ["
          "  {lat: 47.5146, lon: 19.0435, id: 'A1B2', famStat: 'Rendben', monStat: 'OK', days: 5, err: false, bat: 100, year: 2024, critical: false},"
          "  {lat: 47.5155, lon: 19.0412, id: 'C3D4', famStat: 'Ellenőrzés', monStat: 'Gyenge jel', days: 2, err: false, bat: 50, year: 2023, critical: false},"
          "  {lat: 47.5132, lon: 19.0458, id: 'E5F6', famStat: 'Etetés', monStat: 'Alacsony akku (10%)', days: 1, err: false, bat: 10, year: 2022, critical: false},"
          "  {lat: 47.5121, lon: 19.0405, id: 'G7H8', famStat: 'Atkakezelés', monStat: 'Szenzor hiba', days: 0, err: true, bat: 90, year: 2021, critical: false},"
          "  {lat: 47.5150, lon: 19.0495, id: 'DEAD', famStat: '🔥 Hans', monStat: 'OFFLINE', days: 'hans', err: true, bat: 0, year: 2023, critical: true}"
          "];"

          "hivesData.forEach(function(h) {"
          "  var m = L.marker([h.lat, h.lon], {icon: createSquareHiveIcon(h.days, h.year, h.critical, h.err, h.bat)}).addTo(hiveMap)"
          "    .bindPopup('<b>Kaptár: ' + h.id + '</b><br>Család: ' + h.famStat + '<br>Monitor: ' + h.monStat);"
          "  markers.push(m);"
          "});"

          "if(markers.length > 0) {"
          "  var group = L.featureGroup(markers);"
          "  hiveMap.fitBounds(group.getBounds().pad(0.2));"
          "}"

          "</script></div>";

  html += "<div class='card wide'><h2>Állapot és Beavatkozási Ütemterv</h2>";
  html += "<p class='hint'>Sárga: 3 napon túl | Narancs: 3 napon belül | Piros: Holnap | Ciklámen: Ma | 🔥 Hans: Kritikus.</p>";
  html += "<div class='hive-table-container'>";
  html += "<table class='hive-table'>";
  html += "<thead><tr>"
          "<th>Azonosító</th>"
          "<th>Család állapota</th>"
          "<th>Monitor állapota</th>"
          "<th>Beavatkozás</th>"
          "</tr></thead>";
  html += "<tbody>";

  html += "<tr><td>A1B2</td><td>Rendben</td><td>OK</td><td><span class='badge b-yell'>5 nap múlva</span></td></tr>";
  html += "<tr><td>C3D4</td><td>Ellenőrzés</td><td>Jelerősség gyenge</td><td><span class='badge b-org'>2 nap múlva</span></td></tr>";
  html += "<tr><td>E5F6</td><td>Etetés</td><td><span style='color:var(--err)'>Alacsony akku (10%)</span></td><td><span class='badge b-red'>Holnap</span></td></tr>";
  html += "<tr><td>G7H8</td><td>Atkakezelés</td><td><span style='color:var(--err)'>Szenzor olvasási hiba</span></td><td><span class='badge b-cyc'>Ma (Azonnal)</span></td></tr>";
  html += "<tr><td>DEAD</td><td><span class='badge b-flame'>🔥 Hans</span></td><td>OFFLINE</td><td><span class='badge b-flame'>🔥 Hans</span></td></tr>";

  html += "</tbody></table></div></div>";
  html += htmlFoot();
  
  server.send(200, "text/html", html);
} 

void handleExpert() {
  if (!checkPinGuard()) return;
  String html = htmlHead("Expert Konfig", "8");

  html += "<div class='card wide'>"
          "<form action='/expertpost' method='POST'>"
          "<label>Modem hálózati mód (AT+CNMP)</label>"
          "<select name='cnmp'>"
          "<option value='2'>2G / GSM only</option>"
          "<option value='13'>GSM only (Alternatív)</option>"
          "<option value='38' selected>LTE-M / Cat-M (Auto)</option>"
          "<option value='51'>NB-IoT only</option>"
          "<option value='2'>Automatikus (Auto)</option>"
          "</select>"
          "<label>SMS útvonal (AT+CGSMS)</label>"
          "<select name='cgsms'>"
          "<option value='0'>0 - Csak PS (Csomagkapcsolt / LTE)</option>"
          "<option value='1' selected>1 - Csak CS (Áramkörkapcsolt / 2G)</option>"
          "<option value='2'>2 - PS preferred</option>"
          "<option value='3'>3 - CS preferred</option>"
          "</select>"
          "<label>LTE sávok engedélyezése (AT+CBANDCFG - Telekom: B3, B8, B20)</label>"
          "<div class='cb-row'><input type='checkbox' name='b3' id='b3Cb' checked><label for='b3Cb'>Band 3 (1800 MHz)</label></div>"
          "<div class='cb-row'><input type='checkbox' name='b8' id='b8Cb' checked><label for='b8Cb'>Band 8 (900 MHz)</label></div>"
          "<div class='cb-row'><input type='checkbox' name='b20' id='b20Cb' checked><label for='b20Cb'>Band 20 (800 MHz - Legfontosabb vidéken)</label></div>"
          "<label>IoT hálózati technológia (AT+CMNB)</label>"
          "<select name='cmnb'>"
          "<option value='1' selected>1 - Cat-M (LTE-M)</option>"
          "<option value='2'>2 - NB-IoT</option>"
          "<option value='3'>3 - Cat-M és NB-IoT kombinált</option>"
          "</select>"
          "<div style='display:flex;gap:10px;margin-top:20px'>"
          "<button type='submit' style='flex:2'>OK (Elküldés és mentés)</button>"
          "<button type='button' class='sec' onclick='location.href=\"/expertreset\"' style='flex:1'>Visszaállít (Alapértelmezett)</button>"
          "</div>"
          "</form>"
          "<hr style='border:0; border-top:1px solid var(--border); margin:20px 0;'>"
          "<h2>Teljes gyári reset</h2>"
          "<p class='hint'>Minden modem expert beállítás visszaállítása gyári alapértelmezettre (AT&F).</p>"
          "<form action='/expertfullreset' method='POST'>"
          "<button class='danger' style='margin-top:8px'>Teljes Reset (AT&F)</button>"
          "</form>"
          "</div>";
  html += "<form action='/esprestart' method='POST' style='margin-top:10px'>"
          "<button class='danger'>ESP32 Teljes Újraindítás</button></form>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleExpertPost() {
  if(sendModemBusyPage("Expert Mentes", "8", "/expert")) return;

  String cnmp = server.hasArg("cnmp") ? server.arg("cnmp") : "";
  String cgsms = server.hasArg("cgsms") ? server.arg("cgsms") : "";
  String cmnb = server.hasArg("cmnb") ? server.arg("cmnb") : "";
  
  String bands = "1";
  if(server.hasArg("b3")) bands += ",3";
  if(server.hasArg("b8")) bands += ",8";
  if(server.hasArg("b20")) bands += ",20";

  String resultLog = modemApplyExpertConfig(cnmp, cgsms, bands, cmnb);

  String html = htmlHead("Expert Mentes", "8");
  html += "<h1>Expert Konfiguráció Eredménye</h1>";
  html += "<div class='card wide'><div class='diag'>";
  html += resultLog;
  html += "</div><a href='/expert'><button class='sec' style='margin-top:14px'>Vissza az Expert oldalra</button></a></div>";
  html += htmlFoot();
  
  diagAdd("Expert AT konfiguráció elküldve.");
  server.send(200, "text/html", html);
}

void handleExpertReset() {
  if(sendModemBusyPage("Expert Reset", "8", "/expert")) return;
  
  modemResetExpertConfig();

  diagAdd("Expert beállítások visszaállítva gyári alapértelmezettre.");
  server.sendHeader("Location", "/expert");
  server.send(302);
}

void handleCfg() {
  String html = htmlHead("Beallitasok", "4");

  html += "<div class='card wide'><h2>WiFi halozatra csatlakozas</h2>";

  if(gSta.mode == NetMode::STA_CONNECTED) {
    html += "<div class='msg ok'>Csatlakozva: <b>" + htmlEscape(gSta.targetSSID) + "</b><br>"
           "IP cim: " + gSta.ip + "</div>"
            "<form action='/stadisconnect' method='POST'>"
            "<button class='sec'>Kliens mod elhagyasa (vissza AP-ra)</button></form>";
  }
  else if(gSta.mode == NetMode::STA_CONNECTING) {
    html += "<div class='msg warn'>Csatlakozas folyamatban: " + htmlEscape(gSta.targetSSID) + "...</div>"
            "<meta http-equiv='refresh' content='3'>";
  }
  else {
    if(gSta.mode == NetMode::STA_FAILED && gSta.lastError.length()) {
      html += "<div class='msg err'>" + htmlEscape(gSta.lastError) + "</div>";
    }

    html += "<div id='netList'>";
    if(gScanCount == 0) {
      html += "<p class='hint'>Meg nincs lekerdezve halozatlista.</p>";
    } else {
      for(int i=0; i<gScanCount; i++) {
        int pct = constrain((gScanResults[i].rssi + 100) * 2, 0, 100);
        html += "<div class='netitem' onclick='pickNet(\"" + jsEscape(gScanResults[i].ssid) + "\"," +
                String(gScanResults[i].secure ? "true" : "false") + ")'>";
        html += "<span class='netname'>" + htmlEscape(gScanResults[i].ssid) + "</span>";
        html += "<span class='netmeta'>";
        if(gScanResults[i].secure) html += "🔒 ";
        html += String(pct) + "%</span>";
        html += "</div>";
      }
    }
    html += "</div>";

    html += "<button type='button' class='sec' onclick='doScan()' id='scanBtn'>"
            "🔄 Halozatok keresese</button>";

    html += "<div id='pwPopup' style='display:none;margin-top:10px'>"
            "<label id='pwLabel'>Jelszo</label>"
            "<input type='password' id='pwInput' placeholder='WiFi jelszo' autocomplete='off'>"
            "<button onclick='doConnect()' id='connectBtn'>Csatlakozas</button>"
            "<button type='button' class='sec' onclick='cancelPick()' style='margin-top:6px'>Megse</button>"
            "</div>";

    html += R"js(<style>
.netitem{display:flex;justify-content:space-between;align-items:center;
  padding:10px 12px;background:#0a0a18;border:1px solid var(--border);
  border-radius:10px;margin-bottom:6px;cursor:pointer;transition:.15s;gap:12px}
.netitem:active{background:#141428}
.netitem.picked{border-color:var(--accent)}
.netname{font-size:13px;color:var(--txt);overflow-wrap:anywhere}
.netmeta{font-size:11px;color:var(--txt2);white-space:nowrap}
</style>
<script>
var pickedSSID = null, pickedSecure = false;
function pickNet(ssid, secure){
  pickedSSID = ssid; pickedSecure = secure;
  document.getElementById('pwLabel').innerText = 'Jelszo (' + ssid + ')';
  document.getElementById('pwInput').value = '';
  document.getElementById('pwInput').style.display = secure ? 'block' : 'none';
  document.getElementById('pwPopup').style.display = 'block';
  document.getElementById('pwPopup').scrollIntoView({behavior:'smooth', block:'nearest'});
}
function cancelPick(){
  pickedSSID = null;
  document.getElementById('pwPopup').style.display = 'none';
}
function doConnect(){
  if(!pickedSSID) return;
  var pass = pickedSecure ? document.getElementById('pwInput').value : '';
  if(pickedSecure && pass.length < 8){
    alert('A jelszo legalabb 8 karakter (WPA2 minimum).');
    return;
  }
  var btn = document.getElementById('connectBtn');
  btn.disabled = true; btn.innerText = 'Csatlakozas...';
  var body = 'ssid=' + encodeURIComponent(pickedSSID) + '&pass=' + encodeURIComponent(pass);
  fetch('/staconnect', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:body})
    .then(function(r){ return r.text(); })
    .then(function(txt){
      if(txt === 'call-active'){
        alert('Aktiv hivas alatt nem lehet WiFi-t valtani.');
        btn.disabled=false; btn.innerText='Csatlakozas';
        return;
      }
      location.reload();
    })
    .catch(function(){ btn.disabled=false; btn.innerText='Csatlakozas'; });
}
function doScan(){
  var btn = document.getElementById('scanBtn');
  btn.disabled = true; btn.innerText = 'Kereses...';
  fetch('/wifiscan', {method:'POST'})
    .then(function(r){ return r.text(); })
    .then(function(txt){
      if(txt === 'call-active'){
        alert('Aktiv hivas alatt nem lehet halozatot keresni.');
        btn.disabled=false; btn.innerText='Halozatok keresese';
        return;
      }
      location.reload();
    })
    .catch(function(){ btn.disabled=false; btn.innerText='Halozatok keresese'; });
}
</script>)js";
  }
  html += "</div>";

  html += "<div class='card wide'><h2>WiFi AP</h2>"
          "<form action='/savewifi' method='POST'>"
          "<label>SSID vege (elotag: KB-teszt-)</label>"
          "<input type='text' name='ssid' value='";
  String macPart = gApSSID.length()>9 ? gApSSID.substring(9) : "";
  html += macPart;
  html += "' maxlength='20'>"
          "<label>Jelszo (min. 8 kar.)</label>"
          "<input type='password' name='pass' value='' placeholder='ures = valtozatlan' maxlength='31'>"
          "<label>Csatorna</label>"
          "<select name='ch'>";
  for(int i=1;i<=13;i++){
    html += "<option value='" + String(i) + "'" + (i==gApChannel ? " selected" : "") + ">Csatorna " + String(i) + "</option>";
  }
  html += "</select><button>Mentes & ujraindulas</button></form></div>";

  html += "<div class='card wide'><h2>ntfy Beállítások (Üzenetcsatorna)</h2>"
          "<form action='/save-ntfy' method='POST'>"
          "<label>ntfy Szerver</label>"
          "<input type='text' name='ntfy_server' value='" + gNtfyServer + "'>"
          "<label>Topic neve (egyedi azonosító)</label>"
          "<input type='text' name='ntfy_topic' value='" + gNtfyTopic + "' required>"
          "<label>Eszközazonosító (Név, pl. szerver-1)</label>"
          "<input type='text' name='ntfy_nickname' value='" + gNtfyNickname + "'>"
          
          "<div style='display:flex; align-items:center; justify-content:space-between; margin-top:15px; padding-top:10px; border-top:1px solid var(--border);'>"
          "<span>Rendszerindulási tesztüzenet</span>"
          "<label class='sens-toggle' style='--sens-color:var(--ok); margin:0;'>"
          "<input type='checkbox' name='ntfy_startup'" + String(gNtfyStartupMsg ? " checked" : "") + ">"
          "<span class='slider'></span></label>"
          "</div>"
          
          "<button style='margin-top:20px'>ntfy Mentés</button>"
          "</form></div>";

  if (server.hasArg("pin_ok") && gLastValidPin.length() > 0) {
    html += "<div class='card wide' style='border-color:var(--ok);'>"
           "<h2>🎉 SIM sikeresen feloldva!</h2>"
           "<p class='hint'>A megadott PIN kód helyesnek bizonyult. Szeretnéd XTEA-val titkosítva elmenteni, hogy a jövőben automatikusan csatlakozzon?</p>"
           "<form action='/confirmsavepin' method='POST'>"
           "<input type='hidden' name='confirmed_pin' value='" + gLastValidPin + "'>"
           "<button style='background:var(--ok); margin-top:10px;'>Igen, mentés XTEA titkosítással</button>"
           "</form></div>";
  } else if (server.hasArg("pin_err")) {
    html += "<div class='card wide' style='border-color:var(--err);'>"
           "<h2>❌ Hibás PIN kód</h2>"
           "<p class='hint' style='color:var(--err);'>A megadott PIN kóddal a SIM kártya elutasította a bejelentkezést.</p></div>";
  }

  html += "<div class='card'><h2>SIM PIN teszt & mentés</h2>"
          "<form action='/testsavepin' method='POST'>"
          "<label>PIN kód (4-8 szám)</label>"
          "<input type='password' name='pin' id='pi' maxlength='8' "
          "pattern='[0-9]{4,8}' placeholder='pl. 1234' oninput='pc()'>"
          "<button type='submit' id='pb' disabled>PIN tesztelése</button>"
          "</form>"
          "<script>"
          "function pc(){var v=document.getElementById('pi').value;"
          "document.getElementById('pb').disabled=(v.length<4||!/^\\d+$/.test(v));}"
          "</script></div>";

  html += "<div class='card'><h2>SIM PIN csere</h2>"
          "<form action='/changepin' method='POST'>"
          "<label>Jelenlegi PIN</label><input type='password' name='op' id='op' maxlength='8' oninput='cc()'>"
          "<label>Uj PIN</label><input type='password' name='np1' id='np1' maxlength='8' oninput='cc()'>"
          "<label>Uj PIN megint</label><input type='password' name='np2' id='np2' maxlength='8' oninput='cc()'>"
          "<div class='hint' id='ch'></div>"
          "<button type='submit' id='cb' disabled>PIN csere</button></form>"
          "<script>"
          "function cc(){"
          "var o=document.getElementById('op').value,n1=document.getElementById('np1').value,n2=document.getElementById('np2').value,h=document.getElementById('ch'),b=document.getElementById('cb'),d=/^\\d+$/;"
          "b.disabled=true;h.style.color='var(--err)';"
          "if(o.length<4||!d.test(o)){h.innerText='Jelenlegi PIN: min. 4 szam.';return;}"
          "if(n1.length<4||!d.test(n1)){h.innerText='Uj PIN: min. 4 szam.';return;}"
          "if(o===n1){h.innerText='Az uj nem egyezhet a regivel!';return;}"
          "if(n1!==n2){h.innerText='A ket uj PIN nem egyezik!';return;}"
          "h.style.color='var(--ok)';h.innerText='Rendben.';b.disabled=false;}"
          "</script></div>";

  html += "<div class='card wide'><h2>LED / Panelverzio</h2>"
          "<form action='/savepanelver' method='POST'>"
          "<label>Panelverzio</label>"
          "<select name='ver' id='verSel' onchange='verChg()'>"
          "<option value='0'" + String(gLed.mode == 0 ? " selected" : "") + ">V1.0 (GPIO12)</option>"
          "<option value='1'" + String(gLed.mode == 1 ? " selected" : "") + ">V1.1 (GPIO13)</option>"
          "<option value='2'" + String(gLed.mode == 2 ? " selected" : "") + ">Egyeni GPIO</option>"
          "<option value='3'" + String(gLed.mode == 3 ? " selected" : "") + ">AT halozati LED</option>"
          "</select>"
          "<div id='customRow' style='display:" + String(gLed.mode == 2 ? "block" : "none") + "'>"
          "<label>Egyeni GPIO szam</label>"
          "<input type='text' name='custompin' value='" + String(gLed.customPin) + "' maxlength='2' inputmode='numeric'></div>"
          "<button>Mentes</button></form>"
          "<script>"
          "function verChg(){document.getElementById('customRow').style.display=(document.getElementById('verSel').value=='2')?'block':'none';}"
          "</script>"
          "<button id='ledTrigBtn' onclick='ledTrig()' class='" + String(gLed.triggerOn ? "danger" : "sec") + "' style='margin-top:6px'>"
          + String(gLed.triggerOn ? "💡 LED KIKAPCSOLAS" : "💡 LED BEKAPCSOLAS (trigger)") + "</button>"
          "<div class='hint' style='margin-top:6px' id='ledTrigHint'>Uzemmod: <b>"
          + String(gLed.manualOverride ? (gLed.triggerOn ? "MANUALIS - bekapcsolva" : "MANUALIS - kikapcsolva") : "Automatikus") + "</b></div>"
          "<button class='sec' onclick='ledAuto()' style='margin-top:6px'>Vissza automatikus villogasra</button>"
          "<script>"
          "function ledTrig(){"
            "var btn=document.getElementById('ledTrigBtn');btn.disabled=true;"
            "fetch('/ledtrigger',{method:'POST'}).then(function(r){return r.json();}).then(function(d){"
              "var hint=document.getElementById('ledTrigHint');"
              "if(d.on){btn.className='danger';btn.innerHTML='💡 LED KIKAPCSOLAS';hint.innerHTML='Uzemmod: <b>MANUALIS - bekapcsolva</b>';}"
              "else{btn.className='sec';btn.innerHTML='💡 LED BEKAPCSOLAS (trigger)';hint.innerHTML='Uzemmod: <b>MANUALIS - kikapcsolva</b>';}"
              "btn.disabled=false;"
            "}).catch(function(){btn.disabled=false;});"
          "}"
          "function ledAuto(){fetch('/ledauto',{method:'POST'}).then(function(){location.reload();});}"
          "</script></div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleTestSavePin() {
  if(!server.hasArg("pin")){ server.sendHeader("Location","/cfg"); server.send(302); return; }
  String pin = server.arg("pin"); pin.trim();
  
  modem.simUnlock(pin.c_str());
  delay(1200);

  int simStat = modem.getSimStatus();
  if (simStat == 1 /* SIM_READY */) {
    gLastValidPin = pin;
    diagAdd("SIM PIN teszt SIKERES.");
    server.sendHeader("Location", "/cfg?pin_ok=1");
  } else {
    gLastValidPin = "";
    diagAdd("SIM PIN teszt SIKERTELEN. (Kód: " + String(simStat) + ")");
    server.sendHeader("Location", "/cfg?pin_err=1");
  }
  server.send(302);
}

void handleConfirmSavePin() {
  if(server.hasArg("confirmed_pin") && server.arg("confirmed_pin") == gLastValidPin && gLastValidPin.length() > 0) {
    savePin(gLastValidPin); 
    diagAdd("PIN sikeresen elmentve XTEA titkosítással.");
    gLastValidPin = "";
    gModemInitRequested = true;
    sendWaitPage("Modem Inicializálás", "A PIN kód biztonságosan elmentve. A modem újracsatlakozása folyamatban...", "/", 30);
    return;
  }
  server.sendHeader("Location", "/cfg");
  server.send(302);
}

void handleSavePanelVer() {
  if(!server.hasArg("ver")){ server.sendHeader("Location","/cfg"); server.send(302); return; }
  int ver = server.arg("ver").toInt();
  if(ver < 0 || ver > 3) ver = 0;

  int customPin = gLed.customPin;
  if(ver == 2 && server.hasArg("custompin")) {
    int cp = server.arg("custompin").toInt();
    if(cp >= 2 && cp <= 39) customPin = cp;
  }

  if(gLed.mode != 3) {
    int oldPin = currentLedGpio();
    if(oldPin >= 0) digitalWrite(oldPin, LOW);
  } else if(gLed.triggerOn) {
    setNetLightAT(false);
  }

  gLed.mode      = (uint8_t)ver;
  gLed.customPin = (uint8_t)customPin;
  gLed.triggerOn = false;       
  gLed.manualOverride = false;  
  saveLedConfig();
  ledPinReinit();

  diagAdd("Panelverzio/LED mod mentve: mode="+String(ver)+" pin="+String(customPin));
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleLedTrigger() {
  ledTrigger();
  diagAdd(String("LED trigger: ")+(gLed.triggerOn?"BE":"KI")+" (manualis)");
  String json = String("{\"on\":") + (gLed.triggerOn ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleLedAuto() {
  ledSetAuto();
  diagAdd("LED: vissza automatikus modba.");
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleWifiScan() {
  if(gModem.callActive) { server.send(200, "text/plain", "call-active"); return; }
  wifiScan();
  diagAdd("WiFi scan: " + String(gScanCount) + " halozat talalva");
  server.send(200, "text/plain", "ok");
}

void handleStaConnect() {
  if(gModem.callActive) { server.send(200, "text/plain", "call-active"); return; }
  if(!server.hasArg("ssid")) { server.send(400, "text/plain", "hianyzo ssid"); return; }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();
  diagAdd("WiFi STA csatlakozas inditva: " + ssid);
  server.send(200, "text/plain", "ok");
  wifiStaConnect(ssid, pass);
}

void handleStaDisconnect() {
  diagAdd("WiFi STA mod elhagyasa (manualis)");
  wifiStaDisconnect();
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleSaveWifi() {
  if(!server.hasArg("ssid")||!server.hasArg("pass")||!server.hasArg("ch")){
    server.sendHeader("Location","/cfg"); server.send(302); return;
  }
  String suffix = server.arg("ssid"); suffix.trim();
  String pass   = server.arg("pass"); pass.trim();
  int    ch     = server.arg("ch").toInt();

  if(suffix.length()>0) gApSSID = "KB-teszt-" + suffix;
  if(pass.length()>=8){ gApPass=pass; saveApPass(pass); }
  gApChannel = ch;
  EEPROM.write(ADDR_CHANNEL, ch); EEPROM.commit();

  diagAdd("WiFi mentve: "+gApSSID+" ch"+String(ch));
  server.sendHeader("Location","/cfg");
  server.send(302);
  delay(500);
  WiFi.softAPdisconnect(true); delay(200);
  WiFi.softAP(gApSSID.c_str(), gApPass.c_str(), gApChannel);
}

void handleSavePin() {
  if(!server.hasArg("pin")){server.sendHeader("Location","/cfg");server.send(302);return;}
  String pin = server.arg("pin"); pin.trim();
  savePin(pin);
  diagAdd("PIN mentve, modem ujraindul...");
  gModemInitRequested = true;
  
  sendWaitPage("Modem Inicializálás", "A PIN kód mentve. A SIM7000G IoT modem hálózatkeresése szekvenciális, ami nagyjából fél percet vesz igénybe.", "/", 35);
}

void handleChangePin() {
  if(!server.hasArg("op")||!server.hasArg("np1")||!server.hasArg("np2")){
    server.sendHeader("Location","/cfg"); server.send(302); return;
  }
  String op=server.arg("op");op.trim();
  String n1=server.arg("np1");n1.trim();
  String err = changeSIMPin(op, n1);
  if(err.length()==0) diagAdd("SIM PIN megvaltoztatva.");
  else diagAdd("SIM PIN csere HIBA: "+err);
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleDiag() {
  String html = htmlHead("Diagnosztika", "5");

  html += "<script>"
          "function copyElement(id){"
            "var e=document.getElementById(id);"
            "if(!e)return;"
            "var text = e.innerText;"
            "navigator.clipboard.writeText(text).then(function() {"
              "alert('Vágólapra másolva!');"
            "}).catch(function(err) {"
              "console.error('Hiba a másolásnál: ', err);"
              "alert('Másolás sikertelen.');"
            "});"
          "}"
          "</script>";
        
  html += "<div class='card'><h2>Rendszer</h2>";
  html += stateRow("Free heap", String(ESP.getFreeHeap()/1024)+" KB");
  html += stateRow("Uptime", String(millis()/60000)+" perc");
  html += stateRow("NTP", gTime.synced ? gTime.localTime : (gTime.started ? "folyamatban" : "nem indult"), gTime.synced ? "g" : "y");
  html += stateRow("Init kiserletek", String(gModem.initAttempts));
  html += stateRow("AP SSID", gApSSID);
  html += stateRow("AP csatorna", String(gApChannel));
  html += stateRow("Kapcsolodott", String(WiFi.softAPgetStationNum())+" eszkoz");
  html += "</div>";

  if(gModem.ready){
    html += "<div class='card'><h2>Modem</h2>";
    html += stateRow("IMEI", "<span style='font-size:11px'>"+gModem.simIMEI+"</span>");
    html += stateRow("CCID", "<span style='font-size:11px'>"+gModem.simCCID+"</span>");
    html += stateRow("Operator", gModem.operatorName);
    html += stateRow("Jel (raw)", String(gModem.signalQuality));
    html += stateRow("Halozat tipus", gModem.netType);
    html += "</div>";
  }

  html += "<div class='card diag-card'><h2>Esemenyek <button class='sec' style='padding:4px 8px;font-size:11px;float:right;margin-top:-2px' onclick='copyElement(\"diagBox\")'>Másolás</button></h2>";
  html += "<div class='diag' id='diagBox'>";
  String log = diagDump();
  if(log.length()==0) log = "(meg nincs esemeny)";
  html += log;
  html += "</div></div>";

  html += "<div class='card wide'><h2>Beallitasok mentese & visszatoltese</h2>";
  html += "<a href='/eeprombackup'><button class='sec'>Beallitasok exportalasa</button></a>";
  html += "<form action='/eepromrestore' method='POST' style='margin-top:10px'>"
          "<label>Visszatoltendo adat</label>"
          "<textarea name='data' placeholder='Illeszd be ide az exportalt szoveget' "
          "style='min-height:60px;font-family:monospace;font-size:11px'></textarea>"
          "<button class='warn'>Visszatoltes</button>"
          "</form></div>";

  html += "<div class='card wide'><h2>AT parancs</h2>"
          "<label>Parancs</label>"
          "<div style='display:flex;gap:8px'>"
          "<input type='text' id='atCmdInput' placeholder='pl. AT+CSQ' autocomplete='off' autocapitalize='none' style='flex:1'>"
          "<button class='sec' type='button' onclick='sendAtCmd()' style='width:120px;margin-top:0'>Küldés</button>"
          "</div>"
          "<div id='atResultCard' style='display:none;margin-top:10px'>"
          "<label>Válasz <button class='sec' style='padding:2px 6px;font-size:10px;float:right;margin-top:-2px' onclick='copyElement(\"atResultBox\")'>Másolás</button></label>"
          "<div class='diag' id='atResultBox'></div>"
          "</div>"
          "<script>"
          "function sendAtCmd(){"
            "var input = document.getElementById('atCmdInput');var cmd = input.value;if(!cmd) return;"
            "var boxCard = document.getElementById('atResultCard');var box = document.getElementById('atResultBox');"
            "boxCard.style.display = 'block';box.innerText = 'Küldés folyamatban...';"
            "fetch('/at_ajax?cmd=' + encodeURIComponent(cmd))"
            ".then(function(r){ return r.text(); })"
            ".then(function(txt){ box.innerText = txt; input.value = ''; })"
            ".catch(function(){ box.innerText = 'Hiba történt.'; });"
          "}"
          "document.getElementById('atCmdInput').addEventListener('keydown', function(e){if(e.key==='Enter'){e.preventDefault();sendAtCmd();}});"
          "</script>";
  html += "<form action='/atstatus' method='POST' style='margin-top:10px'>"
          "<button class='warn'>AT allapot snapshot</button></form>";
  if(gAtStatusSnapshotAt > 0) html += "<div class='hint'>Legutobbi snapshot: " + ageText(gAtStatusSnapshotAt) + "</div>";
  html += "</div>";

  if(gAtStatusSnapshot.length() > 0) {
    html += "<div class='card diag-card'>"
            "<h2>Legutobbi AT allapot snapshot <button class='sec' style='padding:4px 8px;font-size:11px;float:right;margin-top:-2px' onclick='copyElement(\"atSnapshotBox\")'>Masolas</button></h2>"
            "<div class='diag' id='atSnapshotBox'>" + htmlEscape(gAtStatusSnapshot) + "</div></div>";
  }

  html += "<form action='/reinit' method='POST'>"
          "<button class='warn'>Modem ujraindit</button></form>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleAtAjax() {
  if(!server.hasArg("cmd")) {
    server.send(400, "text/plain", "Hianyzik a parancs");
    return;
  }
  String cmd = normalizeAtCommand(server.arg("cmd"));
  String resp = "";
  if(!gModem.ready) {
    resp = "HIBA: Modem nem aktiv.";
  } else {
    resp = modemAtQuery(cmd, 3000);
    diagAdd(cmd + " -> " + resp.substring(0, 40));
  }
  server.send(200, "text/plain", resp);
}

void handleAtStatus() {
  if(sendModemBusyPage("AT allapot", "5", "/diag")) return;
  diagAdd("AT allapot snapshot inditva");
  refreshAtStatusSnapshot();
  diagAdd("AT allapot snapshot kesz");
  server.sendHeader("Location","/diag");
  server.send(302);
}

void handleModemStatus() {
  String json = "{";
  json += "\"inProgress\":" + String(gModemInitRequested || gModem.initInProgress ? "true" : "false") + ",";
  json += "\"phase\":\"" + jsEscape(gModem.initPhase) + "\",";
  json += "\"phaseNum\":" + String(gModem.initPhaseNum) + ",";
  json += "\"phaseMax\":" + String(ModemState::INIT_PHASE_MAX) + ",";
  json += "\"ready\":" + String(gModem.ready ? "true" : "false") + ",";
  json += "\"uartResponding\":" + String(gModem.uartResponding ? "true" : "false") + ",";
  json += "\"lastError\":\"" + jsEscape(gModem.lastError) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleReinit() {
  if(sendModemBusyPage("Ujraindit", "1", "/")) return;
  diagAdd("Modem ujraindit (manualis)");
  gModem.ready=false; gModem.registered=false;
  gModemInitRequested = true;
  
  sendWaitPage("Modem Újraindítás", "A modem hardveres és szoftveres újraindítása folyamatban van. A hálózati regisztráció befejezéséig kérlek, várj.", "/", 35);
}

void handleExpertFullReset() {
  if (sendModemBusyPage("Teljes Reset", "8", "/expert")) return;
  
  // AT&F parancs a gyári alapértelmezések betöltéséhez, majd mentés
  if (gModem.ready) {
    modemAtQuery("AT&F", 3000);
    modemAtQuery("AT&W", 3000);
  }
  
  diagAdd("Modem teljes gyári reset (AT&F) végrehajtva.");
  server.sendHeader("Location", "/expert");
  server.send(302);
}

void webBegin() {
  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/app.js",        HTTP_GET,  handleJs);
  server.on("/style.css",     HTTP_GET,  handleStyle);
  server.on("/s.css",         HTTP_GET,  handleCss);
  server.on("/api/home",      HTTP_GET,  handleHomeApi);
  server.on("/ntfy-send",     HTTP_POST, handleNtfySend);
  server.on("/ntfy-poll",     HTTP_POST, handleNtfyPoll);
  server.on("/save-ntfy",     HTTP_POST, handleSaveNtfy);
  server.on("/gsm",           HTTP_GET,  handleGsm);
  server.on("/iot",           HTTP_GET,  handleIot);
  
  server.on("/dataon",        HTTP_POST, handleDataOn);
  server.on("/dataoff",       HTTP_POST, handleDataOff);
  server.on("/dataping",      HTTP_POST, handleDataPing);
  
  server.on("/gnss",          HTTP_GET,  handleGnss);
  server.on("/gnssstatus",    HTTP_GET,  handleGnssStatus);
  server.on("/gnssctl",       HTTP_POST, handleGnssCtl);
  server.on("/gnssassist",    HTTP_POST, handleGnssAssist);
  
  server.on("/sensors",       HTTP_GET,  handleSensors);
  server.on("/sensconfig",    HTTP_POST, handleSensConfig);
  server.on("/senstoggle",    HTTP_POST, handleSensToggle);
  server.on("/sensstatus",    HTTP_GET,  handleSensStatus);
  server.on("/senstest",      HTTP_POST, handleSensTest);
  
  server.on("/expert",        HTTP_GET,  handleExpert);
  server.on("/expertpost",    HTTP_POST, handleExpertPost);
  server.on("/expertreset",   HTTP_POST, handleExpertReset);
  server.on("/expertfullreset", HTTP_POST, handleExpertFullReset);

  server.on("/cfg",           HTTP_GET,  handleCfg);
  server.on("/savewifi",      HTTP_POST, handleSaveWifi);
  server.on("/wifiscan",      HTTP_POST, handleWifiScan);
  server.on("/staconnect",    HTTP_POST, handleStaConnect);
  server.on("/stadisconnect", HTTP_POST, handleStaDisconnect);
  server.on("/savepin",       HTTP_POST, handleSavePin);
  server.on("/changepin",     HTTP_POST, handleChangePin);
  server.on("/testsavepin",   HTTP_POST, handleTestSavePin);
  server.on("/confirmsavepin",HTTP_POST, handleConfirmSavePin);
  server.on("/savepanelver",  HTTP_POST, handleSavePanelVer);
  server.on("/ledtrigger",    HTTP_POST, handleLedTrigger);
  server.on("/ledauto",       HTTP_POST, handleLedAuto);
  
  server.on("/diag",          HTTP_GET,  handleDiag);
  server.on("/at_ajax",       HTTP_GET,  handleAtAjax);
  server.on("/atstatus",      HTTP_POST, handleAtStatus);
  server.on("/eeprombackup",  HTTP_GET,  handleEepromBackup);
  server.on("/eepromrestore", HTTP_POST, handleEepromRestore);
  server.on("/setsmsc",       HTTP_POST, handleSetSmsc);
  server.on("/dosms",         HTTP_POST, handleDoSms);
  server.on("/smsstatus",     HTTP_GET,  handleSmsStatus);
  server.on("/docall",        HTTP_POST, handleDoCall);
  server.on("/hangup",        HTTP_POST, handleHangup);
  server.on("/modemstatus",   HTTP_GET,  handleModemStatus);
  server.on("/reinit",        HTTP_POST, handleReinit);

  server.on("/hives",         HTTP_GET, handleHives);
  server.on("/test-report",   HTTP_POST, handleTestReport);
  server.on("/netauto",       HTTP_POST, handleNetAuto);
  server.on("/netscan",       HTTP_POST, handleNetScan);
  server.on("/netmanual",     HTTP_POST, handleNetManual);

  server.on("/esprestart",    HTTP_POST, handleEspRestart);

  server.on("/save-report",   HTTP_POST, handleSaveReport);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("[WEB] Webszerver elindult."));
}