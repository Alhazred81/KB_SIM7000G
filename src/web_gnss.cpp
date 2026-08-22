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
  json += "\"run\":" + String(gGnss.runStatus) + ",";
  json += "\"fixStat\":" + String(gGnss.fixStatus) + ",";
  
  json += "\"lat\":" + String(gGnss.fix ? gGnss.lat : gGnss.assistLat, 6) + ",";
  json += "\"lon\":" + String(gGnss.fix ? gGnss.lon : gGnss.assistLon, 6) + ",";
  json += "\"alt\":" + String(gGnss.alt, 1) + ",";
  json += "\"speed\":" + String(gGnss.speed, 1) + ",";
  json += "\"course\":" + String(gGnss.course, 1) + ",";
  json += "\"hdop\":" + String(gGnss.hdop, 1) + ",";
  
  json += "\"satUsed\":" + String(gGnss.satUsed) + ",";
  json += "\"satView\":\"" + satText(gGnss.satGpsInView) + "\",";
  json += "\"satGPS\":\"" + satText(gGnss.satGPS) + "\",";
  json += "\"satGLO\":\"" + satText(gGnss.satGLO) + "\",";
  json += "\"satBDS\":\"" + satText(gGnss.satBDS) + "\",";
  json += "\"satGAL\":\"" + satText(gGnss.satGAL) + "\",";

  json += "\"poll\":\"" + ageText(gGnss.lastPoll) + "\",";
  json += "\"pos\":\"" + ageText(gGnss.lastPositionPoll) + "\",";
  json += "\"ext\":\"" + ageText(gGnss.lastExtendedPoll) + "\",";
  json += "\"ant\":\"" + ageText(gGnss.lastAntennaPoll) + "\",";
  json += "\"up\":\"" + (gGnss.startedAt ? ageText(gGnss.startedAt) : "meg nem indult") + "\",";
  json += "\"lastFix\":\"" + (gGnss.lastGoodFix ? ageText(gGnss.lastGoodFix) : "meg nem volt") + "\",";
  json += "\"ntp\":\"" + (gTime.synced ? gTime.localTime : "nincs szinkron") + "\",";
  json += "\"utc\":\"" + gGnss.dateStr + " " + gGnss.timeStr + "\",";

  String r1 = gGnss.rawCgnsinf; r1.replace("\r", ""); r1.replace("\n", ""); r1.replace("\"", "\\\"");
  String r2 = gGnss.rawCgnssinfo; r2.replace("\r", ""); r2.replace("\n", ""); r2.replace("\"", "\\\"");
  json += "\"raw1\":\"" + r1 + "\",";
  json += "\"raw2\":\"" + r2 + "\"";
  
  json += "}";
  server.send(200, "application/json", json);
}

void handleGnss() {
  if (!checkPinGuard()) return;
  String html = htmlHead("GPS", "6");

  if(!gModem.ready){
    html += "<div class='msg err'>A modem nincs aktiv, GNSS nem indithato.</div>";
  }

  auto liveRow = [](const String& label, const String& val, const String& id, const String& colorClass = "") {
    String c = colorClass.length() ? (" " + colorClass) : "";
    return "<div class='row'><span class='k'>" + label + "</span><span class='v" + c + "' id='" + id + "'>" + val + "</span></div>";
  };

  double activeLat = gGnss.fix ? gGnss.lat : gGnss.assistLat;
  double activeLon = gGnss.fix ? gGnss.lon : gGnss.assistLon;
  char latS[16], lonS[16];
  dtostrf(activeLat, 0, 6, latS);
  dtostrf(activeLon, 0, 6, lonS);

  // --- 1. OSZLOP (Bal - 340px) ---
  html += "<div style='flex:0 0 auto; width:340px; max-width:100%; display:flex; flex-direction:column; gap:16px;'>";
  
  html += "<div class='card' style='width:100%;'><h2>Vevo allapot</h2>";
  html += stateRow("Vevo", gnssReceiverStatusText(), gGnss.fix ? "g" : (gGnss.enabled ? "y" : "r"));
  html += liveRow("GNSS kapcsolo", gGnss.enabled ? "BE" : "KI", "l_sw", gGnss.enabled ? "g" : "r");
  html += liveRow("Run status", String(gGnss.runStatus), "l_run");
  html += liveRow("Fix status", String(gGnss.fixStatus), "l_fixs");
  html += liveRow("Utolso GNSS poll", ageText(gGnss.lastPoll), "l_poll");
  html += liveRow("Pozicio poll", ageText(gGnss.lastPositionPoll), "l_pos");
  html += liveRow("Muheld poll", ageText(gGnss.lastExtendedPoll), "l_ext");
  html += liveRow("Antenna poll", ageText(gGnss.lastAntennaPoll), "l_ant");
  html += liveRow("Inditas ota", gGnss.startedAt ? ageText(gGnss.startedAt) : "meg nem indult", "l_up");
  html += liveRow("Utolsó fix", gGnss.lastGoodFix ? ageText(gGnss.lastGoodFix) : "meg nem volt", "l_lfix");
  if(gGnss.lastError.length()) html += stateRow("Utolsó hiba", htmlEscape(gGnss.lastError), "y");
  html += liveRow("NTP ido", gTime.synced ? gTime.localTime : "nincs szinkron", "l_ntp", gTime.synced ? "g" : "y");
  html += liveRow("GNSS UTC", gGnss.dateStr + " " + gGnss.timeStr, "l_utc");
  html += "</div>";

  html += "</div>"; 

  // --- 2. OSZLOP (Közép - 696px) ---
  html += "<div style='flex:0 0 auto; width:696px; max-width:100%; display:flex; flex-wrap:wrap; gap:16px; align-content:flex-start;'>";
  
  html += "<div class='card'><h2>Pozicio";
  if(gGnss.fix) html += " <span style='color:var(--ok);font-size:11px'>● FIX</span>";
  else          html += " <span style='color:var(--warn);font-size:11px'>● Nincs fix</span>";
  html += "</h2>";
  html += liveRow("Szelesseg", String(latS)+"°", "l_lat");
  html += liveRow("Hosszusag", String(lonS)+"°", "l_lon");
  html += liveRow("Magassag", String(gGnss.alt,1)+" m", "l_alt");
  html += liveRow("Sebesseg", String(gGnss.speed,1)+" km/h", "l_spd");
  html += liveRow("Irany", String(gGnss.course,1)+"°", "l_crs");
  html += liveRow("HDOP", String(gGnss.hdop,1), "l_hdop");
  html += "</div>";

  html += "<div class='card'><h2>Műholdak</h2>";
  html += liveRow("Osszes hasznalt", String(gGnss.satUsed), "l_su");
  html += liveRow("GPS lathato", satText(gGnss.satGpsInView), "l_sv");
  html += liveRow("GPS", satText(gGnss.satGPS), "l_sgps");
  html += liveRow("GLONASS", satText(gGnss.satGLO), "l_sglo");
  html += liveRow("BeiDou", satText(gGnss.satBDS), "l_sbds");
  html += liveRow("Galileo", satText(gGnss.satGAL), "l_sgal");
  html += "</div>";

  html += "<div class='card wide'><h2>GNSS Debug & Nyers adatok</h2>";
  html += "<div class='diag' id='l_raw' style='min-height:120px; max-height:200px; overflow-y:auto; font-size:11px;'>Betöltés...</div>";
  html += "</div>";

  if(gGnss.enabled) {
      html += "<form action='/gnssctl' method='POST' style='width:100%; margin-top:-4px;'>";
      html += "<input type='hidden' name='action' value='stop'>";
      html += "<button class='danger' style='font-size:15px; padding:12px;'>GNSS kikapcsolasa</button>";
      html += "</form>";
  } else {
      html += "<form action='/gnssctl' method='POST' style='width:100%; margin-top:-4px;'>";
      html += "<input type='hidden' name='action' value='start'>";
      html += "<button style='font-size:15px; padding:12px; background:var(--ok);'>GNSS bekapcsolasa</button>";
      html += "</form>";
  }

  html += "</div>";

  // --- 3. OSZLOP (Jobb - 696px) ---
  html += "<div style='flex:0 0 auto; width:696px; max-width:100%; display:flex; flex-wrap:wrap; gap:16px; align-content:flex-start;'>";

  html += "<div class='card'><h2>Kiindulo koordinata</h2>";
  html += stateRow("Latitude", String(gGnss.assistLat, 6));
  html += stateRow("Longitude", String(gGnss.assistLon, 6));
  html += "<form action='/gnssassist' method='POST'>";
  html += "<label>Latitude</label><input type='text' name='lat' value='" + String(gGnss.assistLat, 6) + "' inputmode='decimal'>";
  html += "<label>Longitude</label><input type='text' name='lon' value='" + String(gGnss.assistLon, 6) + "' inputmode='decimal'>";
  html += "<button class='sec'>Koordinata mentese</button></form>";
  html += "</div>";

  html += "<div class='card'><h2>Pozíció jelentés gyakorisága</h2>";
  html += "<form action='/gnssctl' method='POST'>";
  html += "<p class='hint'>0 = Folyamatosan bekapcsolva.<br>1-255 = X naponta ellenőrzi a pozíciót.</p>";
  html += "<label>Gyakoriság (nap)</label>";
  html += "<input type='number' name='pos_days' min='0' max='255' value='" + String(gPosReportDays) + "'>";
  html += "<button class='sec'>Mentés</button></form></div>";

  html += "<div class='card wide'>"
          "<h2>Térkép</h2>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<div id='map' style='height:260px;border-radius:8px;margin-top:6px;z-index:1'></div>";
          
  char mapUrl[96];
  snprintf(mapUrl, sizeof(mapUrl), "https://maps.google.com/?q=%s,%s", latS, lonS);
  html += "<a href='" + String(mapUrl) + "' target='_blank'><button class='sec' style='margin-top:10px'>🗺 Megnyitas Google Maps-en</button></a>";
  html += "</div>";
  
  html += "</div>";

  // --- JS FRISSÍTŐ SCRIPT ---
  html += "<script>"
          "var osmLayer = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {maxZoom: 19, attribution: '© OpenStreetMap'});"
          "var satLayer = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {maxZoom: 19, attribution: 'Tiles &copy; Esri'});"
          "var map = L.map('map', {center: [" + String(latS) + ", " + String(lonS) + "], zoom: 15, layers: [osmLayer]});"
          "L.control.layers({'Utca': osmLayer, 'Műhold': satLayer}).addTo(map);"
          "var marker = L.marker([" + String(latS) + ", " + String(lonS) + "]).addTo(map)"
            ".bindPopup('" + String(gGnss.fix ? "Aktuális fix" : "Kiinduló hely") + "').openPopup();"
          "var lastLat = " + String(latS) + ", lastLon = " + String(lonS) + ", lastFix = " + String(gGnss.fix ? "true" : "false") + ";"
          
          "function updateUI(){"
            "fetch('/gnssstatus').then(r=>r.json()).then(d=>{"
              "const s = (id, txt, col) => { let e = document.getElementById(id); if(e) { e.innerText = txt; e.className = 'v' + (col ? ' ' + col : ''); } };"
              
              "s('l_sw', d.enabled ? 'BE' : 'KI', d.enabled ? 'g' : 'r');"
              "s('l_run', d.run);"
              "s('l_fixs', d.fixStat);"
              "s('l_poll', d.poll);"
              "s('l_pos', d.pos);"
              "s('l_ext', d.ext);"
              "s('l_ant', d.ant);"
              "s('l_up', d.up);"
              "s('l_lfix', d.lastFix);"
              "s('l_ntp', d.ntp, d.ntp.includes('szinkron') ? 'y' : 'g');"
              "s('l_utc', d.utc);"
              
              "s('l_lat', d.lat + '°');"
              "s('l_lon', d.lon + '°');"
              "s('l_alt', d.alt + ' m');"
              "s('l_spd', d.speed + ' km/h');"
              "s('l_crs', d.course + '°');"
              "s('l_hdop', d.hdop);"
              
              "s('l_su', d.satUsed);"
              "s('l_sv', d.satView);"
              "s('l_sgps', d.satGPS);"
              "s('l_sglo', d.satGLO);"
              "s('l_sbds', d.satBDS);"
              "s('l_sgal', d.satGAL);"
              
              "let rb = document.getElementById('l_raw');"
              "if(rb) rb.innerText = 'Engedélyezve: ' + (d.enabled ? 'BE' : 'KI') + '\\n' +"
                                    "'Fix: ' + (d.fix ? 'VAN' : 'NINCS') + '\\n\\n' +"
                                    "'CGNSINF: ' + d.raw1 + '\\n' +"
                                    "'CGNSSINFO: ' + d.raw2;"
              
              "if(d.fix && (!lastFix || d.lat !== lastLat || d.lon !== lastLon)){"
                "marker.setLatLng([d.lat, d.lon]);"
                "map.setView([d.lat, d.lon], 16);"
                "marker.bindPopup('Aktuális fix').openPopup();"
                "lastLat = d.lat; lastLon = d.lon; lastFix = d.fix;"
              "}"
            "}).catch(()=>{});"
          "}"
          "setInterval(updateUI, 2000);"
          "updateUI();"
          "</script>";

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
  
  if (server.hasArg("pos_days")) {
    uint8_t d = server.arg("pos_days").toInt();
    gnssSaveConfig(d);
    diagAdd("Pozíció jelentés gyakorisága mentve: " + String(d) + " nap");
  } else {
    String action = server.hasArg("action") ? server.arg("action") : "";
    if(action == "start") gnssStart();
    else if(action == "stop") gnssStop();
  }
  
  server.sendHeader("Location","/gnss");
  server.send(302);
}