//web_gnss.cpp

#include "web_gnss.h"
#include "web_common.h"
#include "gnss_mgr.h"
#include "modem_mgr.h"
#include "time_mgr.h"
#include <WebServer.h>

extern WebServer server;
extern GnssState gGnss;
extern ModemState gModem;
extern TimeState gTime;

String satRow(const String& systemName, int count) {
  return stateRow(systemName, satText(count));
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