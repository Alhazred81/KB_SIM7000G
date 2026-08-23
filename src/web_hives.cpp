#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "web_hives.h"
#include "web_common.h"

extern WebServer server;
extern bool checkPinGuard();

void handleHives() {
  if (!checkPinGuard()) return;

  String html = htmlHead("Kaptárak", "9");

  // --- Kétoszlopos CSS Grid és Kártya stílusok ---
  html += "<style>"
          ".hives-layout { display: grid; grid-template-columns: 1fr; gap: 16px; align-items: start; }"
          "@media(min-width: 1000px) { .hives-layout { grid-template-columns: 1fr 1fr; } }"
          ".col-left { display: flex; flex-direction: column; gap: 16px; }"
          ".col-right { display: flex; flex-direction: column; gap: 16px; height: 100%; }"
          
          ".telemetry-grid { display: flex; flex-wrap: wrap; gap: 16px; justify-content: space-around; text-align: center; }"
          ".telemetry-item { display: flex; flex-direction: column; align-items: center; justify-content: center; }"
          ".telemetry-label { font-size: 11px; color: var(--txt2); margin-bottom: 4px; text-transform: uppercase; letter-spacing: 1px; }"
          ".telemetry-value { font-size: 15px; font-weight: bold; color: var(--txt); }"
          ".val-ok { color: var(--ok); }"
          ".val-warn { color: var(--warn); }"
          ".val-err { color: var(--err); }"
          
          ".hive-table { width: 100%; border-collapse: collapse; font-size: 13px; }"
          ".hive-table th { text-align: left; padding: 12px 10px; color: var(--txt2); border-bottom: 1px solid var(--border); font-size:12px; text-transform:uppercase; letter-spacing:1px; }"
          ".hive-table td { padding: 12px 10px; border-bottom: 1px solid rgba(255,255,255,0.05); }"
          ".hive-table tr:last-child td { border-bottom: none; }"
          ".hive-table a { color: var(--accent); text-decoration: none; font-weight: bold; font-size: 14px; transition: 0.2s; }"
          ".hive-table a:hover { filter: brightness(1.2); text-decoration: underline; }"
          
          ".badge { padding: 6px 10px; border-radius: 6px; font-size: 11px; font-weight: bold; text-align: center; display: inline-block; min-width: 90px; }"
          ".b-grn { background: rgba(34,197,94,0.15); color: #22c55e; border: 1px solid #22c55e; }"
          ".b-yell { background: rgba(234,179,8,0.15); color: #eab308; border: 1px solid #eab308; }"
          ".b-org { background: rgba(249,115,22,0.15); color: #f97316; border: 1px solid #f97316; }"
          ".b-red { background: rgba(239,68,68,0.15); color: #ef4444; border: 1px solid #ef4444; }"
          
          "#map { height: 480px; width: 100%; border-radius: 8px; z-index: 1; border: 1px solid var(--border); }"
          "</style>";

  html += "<link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\" />";
  html += "<script src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\"></script>";

  html += "<div class='hives-layout'>";

  // Bal oszlop
  html += "<div class='col-left'>";
  html += "<div class='card full' style='margin:0; padding:16px;'>";
  html += "<div class='telemetry-grid'>";
  html += "<div class='telemetry-item'><span class='telemetry-label'>GSM Térerő</span><span class='telemetry-value' id='tele-gsm'>Frissítés...</span></div>";
  html += "<div class='telemetry-item'><span class='telemetry-label'>GPS Fix</span><span class='telemetry-value' id='tele-gps'>Frissítés...</span></div>";
  html += "<div class='telemetry-item'><span class='telemetry-label'>Uptime</span><span class='telemetry-value' id='tele-up'>Frissítés...</span></div>";
  html += "<div class='telemetry-item'><span class='telemetry-label'>Szabad Memória</span><span class='telemetry-value' id='tele-mem'>Frissítés...</span></div>";
  html += "</div>";
  html += "</div>";

  html += "<div class='card full' style='margin:0; padding:16px;'>";
  html += "<h2 style='font-size:15px; margin-bottom:8px;'>🗺 KAPTÁRAK ÉS KÉSZLETEK TÉRKÉPE</h2>";
  html += "<p class='hint' style='margin-bottom:12px;'>Koppints bármelyik elemre a részletekért.</p>";
  html += "<div id='map'></div>";
  html += "</div>";
  html += "</div>";

  // Jobb oszlop
  html += "<div class='col-right'>";
  html += "<div class='card full' style='margin:0; padding:16px; height:100%;'>";
  html += "<h2 style='font-size:15px; margin-bottom:8px;'>📋 ÁLLAPOT ÉS BEAVATKOZÁSI ÜTEMTERV</h2>";
  html += "<p class='hint' style='margin-bottom:12px;'>Koppints a kaptár azonosítójára a részletes nézethez.</p>";
  
  html += "<div style='overflow-x:auto;'>";
  html += "<table class='hive-table'>";
  html += "<tr><th>Azonosító</th><th>Kapcsolat</th><th>Család állapota</th><th>Státusz</th></tr>";
  html += "<tr><td><a href='/hive?hive=A1B2'>A1B2</a></td><td><span class='val-ok'>📶 -68 dBm</span></td><td>Rendben, Erős</td><td><span class='badge b-grn'>Rendben</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=B3C4'>B3C4</a></td><td><span class='val-ok'>📶 -75 dBm</span></td><td>Fejlesztés alatt</td><td><span class='badge b-yell'>5 nap múlva</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=C5D6'>C5D6</a></td><td><span class='val-warn'>📶 -92 dBm</span></td><td>Ellenőrzés szükséges</td><td><span class='badge b-org'>2 nap múlva</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=D7E8'>D7E8</a></td><td><span class='val-err'>📶 Offline</span></td><td>Etetés esedékes</td><td><span class='badge b-red'>Holnap</span></td></tr>";
  html += "</table>";
  html += "</div>";
  html += "</div>";
  html += "</div>";

  html += "</div>";

  // JavaScript a térképhez és a telemetriához
  html += "<script>"
          "var map = L.map('map').setView([47.514600, 19.043500], 18);"
          
          "L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {"
          "  attribution: 'Tiles &copy; Esri'"
          "}).addTo(map);"
          
          // Kaptárnégyzet ikon generáló függvény
          "var customIcon = function(color) {"
          "  return L.divIcon({"
          "    className: 'custom-div-icon',"
          "    html: '<div style=\"background-color:'+color+'; width:20px; height:20px; border-radius:6px; border:2px solid #fff; box-shadow:0 0 6px rgba(0,0,0,0.6);\"></div>',"
          "    iconSize: [24, 24],"
          "    iconAnchor: [12, 12]"
          "  });"
          "};"

          // Időjárás-állomás és szerver jelölője
          "var stationIcon = L.divIcon({ className: 'station-icon', html: '<div style=\"background:#4d4dff; padding:5px; border-radius:50%; font-size:16px; text-align:center; border:2px solid #fff; box-shadow: 0 0 10px rgba(77,77,255,0.8); display:flex; align-items:center; justify-content:center; width:30px; height:30px;\">📡</div>', iconSize: [44,44], iconAnchor: [22,22] });"
          "L.marker([47.514620, 19.043520], {icon: stationIcon}).bindPopup('<b>Időjárás-állomás és szerver</b>').addTo(map);"

          "fetch('/api/map_status').then(r=>r.json()).then(data=>{"
          "  document.getElementById('tele-gsm').innerHTML = '<span style=\"color:#4d4dff; margin-right:6px; font-size:16px;\">📊</span><span class=\"val-ok\">' + data.signal + ' dBm</span>';"
          "  var gpsText = data.fix ? '<span class=\"val-ok\">Van (' + data.sat + ')</span>' : '<span class=\"val-err\">Nincs</span>';"
          "  document.getElementById('tele-gps').innerHTML = gpsText;"
          "  document.getElementById('tele-up').innerHTML = '<b>' + data.uptime + ' perc</b>';"
          "  document.getElementById('tele-mem').innerHTML = '<b>' + data.heap + ' KB</b>';"

          "  if (data.markers && Array.isArray(data.markers)) {"
          "    data.markers.forEach(item => {"
          "      if (item.type === 'hive') {"
          "        let color = '#22c55e';"
          "        if (item.status === 'warn') color = '#eab308';"
          "        if (item.status === 'org') color = '#f97316';"
          "        if (item.status === 'err' || item.status === 'critical') color = '#ef4444';"
          "        let funcText = item.colonyFunc ? ('<br><span style=\"font-size:11px; color:var(--accent);\">' + item.colonyFunc + '</span>') : '';"
          "        let popupHtml = '<div style=\"text-align:center;\"><b>' + item.id + '</b>' + funcText + '<br><a href=\"/hive?hive=' + item.id + '\">Részletek megnyitása</a></div>';"
          "        L.marker([item.lat, item.lng], {icon: customIcon(color)}).bindPopup(popupHtml).addTo(map);"
          "      } else if (item.type === 'water' || item.type === 'syrup') {"
          "        let borderColor = '#22c55e';"
          "        if (item.status === 'low') borderColor = '#f97316';"
          "        if (item.status === 'critical') borderColor = '#ef4444';"
          "        let bgColor = (item.type === 'water') ? '#3b82f6' : '#eab308';"
          "        let symbol = (item.type === 'water') ? '💧' : '🍬';"
          "        let iconHtml = '<div style=\"width: 36px; height: 42px; background: ' + bgColor + '; border: 3px solid ' + borderColor + '; border-radius: 8px 8px 4px 4px; position: relative; box-shadow: 0 4px 6px rgba(0,0,0,0.4); display: flex; justify-content: center; align-items: center;\"><div style=\"position: absolute; top: -6px; left: 10px; width: 10px; height: 4px; background: ' + borderColor + '; border-radius: 2px;\"></div><div style=\"width: 24px; height: 24px; background: white; border-radius: 50%; display: flex; justify-content: center; align-items: center; font-size: 14px;\">' + symbol + '</div></div>';"
          "        let customDivIcon = L.divIcon({ className: 'custom-map-marker', html: iconHtml, iconSize: [36, 42], iconAnchor: [18, 42] });"
          "        let popupHtml = '<div style=\"text-align:center;\"><b>' + item.id + ' (' + (item.type === 'water' ? 'Itató' : 'Szirup') + ')</b><br>Szint: <b>' + item.level + '%</b><br>Állapot: ' + item.status + '</div>';"
          "        L.marker([item.lat, item.lng], {icon: customDivIcon}).bindPopup(popupHtml).addTo(map);"
          "      }"
          "    });"
          "  }"
          "}).catch(e=>console.log('Térkép API hiba:', e));"
          "</script>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}