//web_hives.cpp

#include "web_hives.h"
#include "web_common.h"
#include <WebServer.h>

extern WebServer server;
extern bool checkPinGuard();



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
