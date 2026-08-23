#include <WebServer.h>
#include <Arduino.h>
#include "web_hives.h"
#include "web_common.h"

extern WebServer server;
extern bool checkPinGuard();

// Bázisállomás / Szerver kártya HTML generálása a térkép alá
String getSzerverMapCardHtml() {
  String html = "<div class='card wide' style='grid-column:1/-1; display:flex; justify-content:space-around; flex-wrap:wrap; gap:15px; text-align:center;'>";
  html += "<div><span class='hint'>GSM Térerő</span><br><b id='map_signal' style='font-size:16px;'>- dBm</b></div>";
  html += "<div><span class='hint'>GPS Fix</span><br><b id='map_fix' style='font-size:16px;'>-</b></div>";
  html += "<div><span class='hint'>Uptime</span><br><b id='map_uptime' style='font-size:16px;'>- perc</b></div>";
  html += "<div><span class='hint'>Szabad Memória</span><br><b id='map_heap' style='font-size:16px;'>- KB</b></div>";
  html += "</div>";
  return html;
}

void handleHives() {
  if (!checkPinGuard()) return;
  String html = htmlHead("Kaptárak", "9");

  html += "<style>"
          "@keyframes flammenwerfer { 0% { opacity: 1; background-color: rgba(255,0,0,0.3); } 50% { opacity: 0.4; background-color: rgba(255,0,0,0.8); } 100% { opacity: 1; background-color: rgba(255,0,0,0.3); } }"
          "@keyframes mapIconPulse { 0% { transform: scale(1); filter: drop-shadow(0 0 2px rgba(255,0,0,0.8)); } 50% { transform: scale(1.15); filter: drop-shadow(0 0 10px rgba(255,0,0,1)); } 100% { transform: scale(1); filter: drop-shadow(0 0 2px rgba(255,0,0,0.8)); } }"
          ".hive-table-container { max-height: 450px; overflow-y: auto; border: 1px solid var(--border); border-radius: 8px; }"
          "table.hive-table { width: 100%; border-collapse: collapse; text-align: left; font-size: 13px; }"
          "table.hive-table th { position: sticky; top: 0; background: var(--nav); color: var(--txt); padding: 12px 10px; border-bottom: 2px solid var(--border); z-index: 2; }"
          "table.hive-table td { padding: 12px 10px; border-bottom: 1px solid var(--border); }"
          ".badge { padding: 6px 10px; border-radius: 6px; font-weight: bold; font-size: 12px; display: inline-block; }"
          ".b-grn { background: rgba(34,197,94,0.2); color: #22c55e; }"
          ".b-yell { background: rgba(234,179,8,0.2); color: #eab308; }"
          ".b-org { background: rgba(249,115,22,0.2); color: #f97316; }"
          ".b-red { background: rgba(239,68,68,0.2); color: #ef4444; }"
          ".b-purp { background: rgba(168,85,247,0.2); color: #a855f7; }"
          ".b-flame { background: rgba(255,0,0,0.5); color: #fff; animation: flammenwerfer 0.8s infinite; }"
          ".custom-hive-icon { background: transparent; border: none; cursor: pointer; }"
          "</style>";

  html += "<div class='card wide' style='grid-column:1/-1'>"
          "<h2>🗺 Kaptárak & Időjárás-állomás Térképe</h2>"
          "<p class='hint'>Koppints bármelyik kaptárra a részletes nézethez.</p>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<div id='hiveMap' style='height:400px;border-radius:8px;margin-top:6px;z-index:1'></div>"
          "<script>"
          "var osmLayerH = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {maxZoom: 19, attribution: '© OpenStreetMap'});"
          "var satLayerH = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {maxZoom: 19, attribution: 'Tiles &copy; Esri'});"
          "var hiveMap = L.map('hiveMap', {center: [47.514600, 19.043500], zoom: 19, layers: [satLayerH]});" 
          "var baseLayersH = {'Utca': osmLayerH, 'Műhold': satLayerH};"
          "L.control.layers(baseLayersH).addTo(hiveMap);"
          
          "function getSzerverIcon(sigColor, batColor) {"
          "  let svg = '<svg viewBox=\"0 0 64 64\" width=\"32\" height=\"32\">' +"
          "    '<line x1=\"20\" y1=\"60\" x2=\"44\" y2=\"60\" stroke=\"#e0e0e0\" stroke-width=\"4\" stroke-linecap=\"round\"/>' +"
          "    '<line x1=\"32\" y1=\"60\" x2=\"32\" y2=\"10\" stroke=\"#e0e0e0\" stroke-width=\"4\" stroke-linecap=\"round\"/>' +"
          "    '<path d=\"M32 20 L14 20 L8 14 L8 26 Z\" fill=\"#e0e0e0\"/>' +"
          "    '<line x1=\"32\" y1=\"10\" x2=\"52\" y2=\"10\" stroke=\"#e0e0e0\" stroke-width=\"4\" stroke-linecap=\"round\"/>' +"
          "    '<line x1=\"52\" y1=\"10\" x2=\"52\" y2=\"16\" stroke=\"#e0e0e0\" stroke-width=\"4\" stroke-linecap=\"round\"/>' +"
          "    '<circle cx=\"52\" cy=\"18\" r=\"4\" fill=\"#e0e0e0\"/>' +"
          "    '<circle cx=\"40\" cy=\"10\" r=\"4\" fill=\"#e0e0e0\"/>' +"
          "    '<rect x=\"40\" y=\"32\" width=\"20\" height=\"20\" rx=\"2\" fill=\"none\" stroke=\"' + batColor + '\" stroke-width=\"3\"/>' +"
          "    '<line x1=\"38\" y1=\"38\" x2=\"62\" y2=\"38\" stroke=\"' + batColor + '\" stroke-width=\"3\" stroke-linecap=\"round\"/>' +"
          "    '<line x1=\"38\" y1=\"46\" x2=\"62\" y2=\"46\" stroke=\"' + batColor + '\" stroke-width=\"3\" stroke-linecap=\"round\"/>' +"
          "    '<line x1=\"32\" y1=\"42\" x2=\"16\" y2=\"42\" stroke=\"' + sigColor + '\" stroke-width=\"3\" stroke-linecap=\"round\"/>' +"
          "    '<line x1=\"16\" y1=\"42\" x2=\"16\" y2=\"34\" stroke=\"' + sigColor + '\" stroke-width=\"3\" stroke-linecap=\"round\"/>' +"
          "    '<path d=\"M10 28 Q16 22 22 28 M4 23 Q16 11 28 23\" fill=\"none\" stroke=\"' + sigColor + '\" stroke-width=\"3\" stroke-linecap=\"round\"/>' +"
          "  '</svg>';"
          "  return L.divIcon({"
          "    className: 'custom-hive-icon',"
          "    html: '<div style=\"background:#141428; border:2px solid var(--accent); border-radius:50%; width:44px; height:44px; display:flex; align-items:center; justify-content:center; box-shadow:0 4px 10px rgba(0,0,0,0.8);\" title=\"Szerver & Időjárás-állomás\">' + svg + '</div>',"
          "    iconSize: [44, 44], iconAnchor: [22, 22]"
          "  });"
          "}"
          
          "var szerverMarker = L.marker([47.5146, 19.0435], {icon: getSzerverIcon('#e0e0e0', '#e0e0e0'), zIndexOffset: 1000}).addTo(hiveMap);"
          
          "var iGreen = L.divIcon({ className: 'custom-hive-icon', html: '<div style=\"background:rgba(34,197,94,0.8); border:2px solid #22c55e; width:26px; height:26px; border-radius:4px; box-shadow:0 2px 6px rgba(0,0,0,0.8);\"></div>', iconSize: [26,26], iconAnchor: [13,13] });"
          "var iYellow = L.divIcon({ className: 'custom-hive-icon', html: '<div style=\"background:rgba(234,179,8,0.8); border:2px solid #eab308; width:26px; height:26px; border-radius:4px; box-shadow:0 2px 6px rgba(0,0,0,0.8);\"></div>', iconSize: [26,26], iconAnchor: [13,13] });"
          "var iOrange = L.divIcon({ className: 'custom-hive-icon', html: '<div style=\"background:rgba(249,115,22,0.8); border:2px solid #f97316; width:26px; height:26px; border-radius:4px; box-shadow:0 2px 6px rgba(0,0,0,0.8);\"></div>', iconSize: [26,26], iconAnchor: [13,13] });"
          "var iRed = L.divIcon({ className: 'custom-hive-icon', html: '<div style=\"background:rgba(239,68,68,0.8); border:2px solid #ef4444; width:26px; height:26px; border-radius:4px; box-shadow:0 2px 6px rgba(0,0,0,0.8);\"></div>', iconSize: [26,26], iconAnchor: [13,13] });"
          "var iPurple = L.divIcon({ className: 'custom-hive-icon', html: '<div style=\"background:rgba(168,85,247,0.8); border:2px solid #a855f7; width:26px; height:26px; border-radius:4px; box-shadow:0 2px 6px rgba(0,0,0,0.8);\"></div>', iconSize: [26,26], iconAnchor: [13,13] });"
          "var iHans = L.divIcon({ className: 'custom-hive-icon', html: '<div style=\"background:rgba(255,0,0,0.8); border:2px solid #ff0000; width:26px; height:26px; border-radius:4px; box-shadow:0 0 12px red; animation: mapIconPulse 0.8s infinite ease-in-out;\"></div>', iconSize: [26,26], iconAnchor: [13,13] });"

          "var hiveA = L.marker([47.51465, 19.04355], {icon: iGreen}).addTo(hiveMap); hiveA.on('click', ()=>location.href='/hive?hive=A1B2');"
          "var hiveB = L.marker([47.51455, 19.04345], {icon: iYellow}).addTo(hiveMap); hiveB.on('click', ()=>location.href='/hive?hive=B3C4');"
          "var hiveC = L.marker([47.51462, 19.04342], {icon: iOrange}).addTo(hiveMap); hiveC.on('click', ()=>location.href='/hive?hive=C5D6');"
          "var hiveD = L.marker([47.51458, 19.04358], {icon: iRed}).addTo(hiveMap); hiveD.on('click', ()=>location.href='/hive?hive=D7E8');"
          "var hiveE = L.marker([47.51468, 19.04348], {icon: iPurple}).addTo(hiveMap); hiveE.on('click', ()=>location.href='/hive?hive=E9F0');"
          "var hiveDEAD = L.marker([47.51452, 19.04352], {icon: iHans}).addTo(hiveMap); hiveDEAD.on('click', ()=>location.href='/hive?hive=DEAD');"

          "function updateSzerverMapLive() {"
          "  fetch('/api/map_status').then(r => r.json()).then(d => {"
          "    let sigColor = d.signal > -85 ? '#22c55e' : (d.signal > -100 ? '#eab308' : '#ef4444');"
          "    let batColor = d.battery >= 3.8 ? '#22c55e' : (d.battery >= 3.5 ? '#eab308' : '#ef4444');"
          "    szerverMarker.setIcon(getSzerverIcon(sigColor, batColor));"
          "    document.getElementById('map_signal').innerHTML = '<span style=\"color:' + sigColor + ';\">📶 ' + d.signal + ' dBm</span>';"
          "    document.getElementById('map_fix').innerHTML = d.fix ? ('<span style=\"color:#22c55e\">Van (' + d.sat + ')</span>') : '<span style=\"color:#ef4444\">Nincs</span>';"
          "    document.getElementById('map_uptime').innerText = d.uptime + ' perc';"
          "    document.getElementById('map_heap').innerText = d.heap + ' KB';"
          "    if(d.lat && d.lon && (d.lat !== 0 || d.lon !== 0)) {"
          "      szerverMarker.setLatLng([d.lat, d.lon]);"
          "      if(!window.mapCentered) { hiveMap.setView([d.lat, d.lon], 19); window.mapCentered = true; }"
          "    }"
          "  }).catch(()=>{});"
          "}"
          "setInterval(updateSzerverMapLive, 5000);"
          "updateSzerverMapLive();"
          "</script></div>";

  html += getSzerverMapCardHtml();

  html += "<div class='card wide'><h2>Állapot és Beavatkozási Ütemterv</h2>";
  html += "<p class='hint'>Koppints a kaptár azonosítójára a részletes nézethez.</p>";
  html += "<div class='hive-table-container'>";
  html += "<table class='hive-table'>";
  html += "<thead><tr><th>Azonosító</th><th>Család állapota</th><th>Státusz</th></tr></thead>";
  html += "<tbody>";
  
  html += "<tr><td><a href='/hive?hive=A1B2' style='color:var(--accent); font-weight:bold; font-size:15px;'>A1B2</a></td><td>Rendben, Erős</td><td><span class='badge b-grn'>Rendben</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=B3C4' style='color:var(--accent); font-weight:bold; font-size:15px;'>B3C4</a></td><td>Fejlesztés alatt</td><td><span class='badge b-yell'>5 nap múlva</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=C5D6' style='color:var(--accent); font-weight:bold; font-size:15px;'>C5D6</a></td><td>Ellenőrzés szükséges</td><td><span class='badge b-org'>2 nap múlva</span></td></tr>";
  html += "<tr><td><a href='/hive;hive=D7E8' style='color:var(--accent); font-weight:bold; font-size:15px;'>D7E8</a></td><td>Etetés esedékes</td><td><span class='badge b-red'>Holnap</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=E9F0' style='color:var(--accent); font-weight:bold; font-size:15px;'>E9F0</a></td><td>Atkakezelés</td><td><span class='badge b-purp'>Ma (Azonnal)</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=DEAD' style='color:var(--accent); font-weight:bold; font-size:15px;'>DEAD</a></td><td>Kritikus probléma</td><td><span class='badge b-flame'>🔥 Hans</span></td></tr>";
  
  html += "</tbody></table></div></div>";
  
  html += htmlFoot();
  server.send(200, "text/html", html);
}