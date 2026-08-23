#include <Arduino.h>
#include <WebServer.h>
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
          /* Nagy monitoron (1000px felett) 1:1 arányban osztjuk el a bal és jobb oszlopot */
          "@media(min-width: 1000px) { .hives-layout { grid-template-columns: 1fr 1fr; } }"
          ".col-left { display: flex; flex-direction: column; gap: 16px; }"
          ".col-right { display: flex; flex-direction: column; gap: 16px; height: 100%; }"
          
          /* Telemetria kártya belső rácsa */
          ".telemetry-grid { display: flex; flex-wrap: wrap; gap: 16px; justify-content: space-around; text-align: center; }"
          ".telemetry-item { display: flex; flex-direction: column; align-items: center; justify-content: center; }"
          ".telemetry-label { font-size: 11px; color: var(--txt2); margin-bottom: 4px; text-transform: uppercase; letter-spacing: 1px; }"
          ".telemetry-value { font-size: 15px; font-weight: bold; color: var(--txt); }"
          ".val-ok { color: var(--ok); }"
          ".val-warn { color: var(--warn); }"
          ".val-err { color: var(--err); }"
          
          /* Táblázat stílusok (Bővítve a Kapcsolat oszloppal) */
          ".hive-table { width: 100%; border-collapse: collapse; font-size: 13px; }"
          ".hive-table th { text-align: left; padding: 12px 10px; color: var(--txt2); border-bottom: 1px solid var(--border); font-size:12px; text-transform:uppercase; letter-spacing:1px; }"
          ".hive-table td { padding: 12px 10px; border-bottom: 1px solid rgba(255,255,255,0.05); }"
          ".hive-table tr:last-child td { border-bottom: none; }"
          ".hive-table a { color: var(--accent); text-decoration: none; font-weight: bold; font-size: 14px; transition: 0.2s; }"
          ".hive-table a:hover { filter: brightness(1.2); text-decoration: underline; }"
          
          /* Státusz badge-ek megegyezően a részletes nézettel */
          ".badge { padding: 6px 10px; border-radius: 6px; font-size: 11px; font-weight: bold; text-align: center; display: inline-block; min-width: 90px; }"
          ".b-grn { background: rgba(34,197,94,0.15); color: #22c55e; border: 1px solid #22c55e; }"
          ".b-yell { background: rgba(234,179,8,0.15); color: #eab308; border: 1px solid #eab308; }"
          ".b-org { background: rgba(249,115,22,0.15); color: #f97316; border: 1px solid #f97316; }"
          ".b-red { background: rgba(239,68,68,0.15); color: #ef4444; border: 1px solid #ef4444; }"
          
          /* Térkép konténer */
          "#map { height: 480px; width: 100%; border-radius: 8px; z-index: 1; border: 1px solid var(--border); }"
          "</style>";

  // --- Leaflet JS és CSS a térképhez ---
  html += "<link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\" />";
  html += "<script src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\"></script>";

  // --- RÁCS KEZDŐDIK ---
  html += "<div class='hives-layout'>";

  // ===================== BAL OSZLOP (Telemetria + Térkép) =====================
  html += "<div class='col-left'>";

  // 1. Telemetria kártya (Felkerült a térkép felé!)
  html += "<div class='card full' style='margin:0; padding:16px;'>";
  html += "<div class='telemetry-grid'>";
  html += "<div class='telemetry-item'><span class='telemetry-label'>GSM Térerő</span><span class='telemetry-value' id='tele-gsm'>Frissítés...</span></div>";
  html += "<div class='telemetry-item'><span class='telemetry-label'>GPS Fix</span><span class='telemetry-value' id='tele-gps'>Frissítés...</span></div>";
  html += "<div class='telemetry-item'><span class='telemetry-label'>Uptime</span><span class='telemetry-value' id='tele-up'>Frissítés...</span></div>";
  html += "<div class='telemetry-item'><span class='telemetry-label'>Szabad Memória</span><span class='telemetry-value' id='tele-mem'>Frissítés...</span></div>";
  html += "</div>";
  html += "</div>";

  // 2. Térkép kártya
  html += "<div class='card full' style='margin:0; padding:16px;'>";
  html += "<h2 style='font-size:15px; margin-bottom:8px;'>🗺 KAPTÁRAK TÉRKÉPE</h2>";
  html += "<p class='hint' style='margin-bottom:12px;'>Koppints bármelyik kaptárra a részletes nézethez.</p>";
  html += "<div id='map'></div>";
  html += "</div>";

  html += "</div>"; // Bal oszlop vége


  // ===================== JOBB OSZLOP (Táblázat) =====================
  html += "<div class='col-right'>";

  // 3. Táblázat kártya (Most már a teljes jobb oldalt kitölti)
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

  html += "</div>"; // Táblázat kártya vége

  html += "</div>"; // Jobb oszlop vége

  html += "</div>"; // hives-layout vége

  // --- JavaScript a térképhez és a telemetriához ---
  html += "<script>"
          "var map = L.map('map').setView([47.514600, 19.043500], 18);"
          
          // Műholdas térképréteg (Esri)
          "L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {"
          "  attribution: 'Tiles &copy; Esri'"
          "}).addTo(map);"
          
          // Egyedi ikon generáló függvény
          "var customIcon = function(color) {"
          "  return L.divIcon({"
          "    className: 'custom-div-icon',"
          "    html: '<div style=\"background-color:'+color+'; width:20px; height:20px; border-radius:6px; border:2px solid #fff; box-shadow:0 0 6px rgba(0,0,0,0.6);\"></div>',"
          "    iconSize: [24, 24],"
          "    iconAnchor: [12, 12]"
          "  });"
          "};"
          
          // --- Kaptárak jelölői ---
          "L.marker([47.514700, 19.043600], {icon: customIcon('#22c55e')}).bindPopup('<div style=\"text-align:center;\"><b>A1B2</b><br><a href=\"/hive?hive=A1B2\">Részletek megnyitása</a></div>').addTo(map);"
          "L.marker([47.514650, 19.043550], {icon: customIcon('#eab308')}).bindPopup('<div style=\"text-align:center;\"><b>B3C4</b><br><a href=\"/hive?hive=B3C4\">Részletek megnyitása</a></div>').addTo(map);"
          "L.marker([47.514600, 19.043500], {icon: customIcon('#f97316')}).bindPopup('<div style=\"text-align:center;\"><b>C5D6</b><br><a href=\"/hive?hive=C5D6\">Részletek megnyitása</a></div>').addTo(map);"
          "L.marker([47.514550, 19.043450], {icon: customIcon('#ef4444')}).bindPopup('<div style=\"text-align:center;\"><b>D7E8</b><br><a href=\"/hive?hive=D7E8\">Részletek megnyitása</a></div>').addTo(map);"
          
          // --- Központi Szerver / Időjárás-állomás jelölője ---
          "var stationIcon = L.divIcon({ className: 'station-icon', html: '<div style=\"background:#4d4dff; padding:5px; border-radius:50%; font-size:16px; text-align:center; border:2px solid #fff; box-shadow: 0 0 10px rgba(77,77,255,0.8); display:flex; align-items:center; justify-content:center; width:30px; height:30px;\">📡</div>', iconSize: [44,44], iconAnchor: [22,22] });"
          "L.marker([47.514620, 19.043520], {icon: stationIcon}).bindPopup('<b>Kaptármonitor Szerver</b>').addTo(map);"

          // --- Telemetria frissítése az API-n keresztül ---
          "fetch('/api/map_status').then(r=>r.json()).then(data=>{"
          "  document.getElementById('tele-gsm').innerHTML = '<span style=\"color:#4d4dff; margin-right:6px; font-size:16px;\">📊</span><span class=\"val-ok\">' + data.signal + ' dBm</span>';"
          "  var gpsText = data.fix ? '<span class=\"val-ok\">Van (' + data.sat + ')</span>' : '<span class=\"val-err\">Nincs</span>';"
          "  document.getElementById('tele-gps').innerHTML = gpsText;"
          "  document.getElementById('tele-up').innerHTML = '<b>' + data.uptime + ' perc</b>';"
          "  document.getElementById('tele-mem').innerHTML = '<b>' + data.heap + ' KB</b>';"
          "}).catch(e=>console.log('Telemetria API hiba:', e));"
          "</script>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}