#include <WebServer.h>
#include <Preferences.h>
#include "web_handlehive.h"
#include "web_common.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

extern WebServer server;
extern bool checkPinGuard();

void handleHiveView() {
  if (!checkPinGuard()) return;
  
  String hiveId = server.hasArg("hive") ? server.arg("hive") : "A1B2";
  
  int queenYear = 2023;
  int batPct = 90;
  String monStat = "OK";

  String famStatus = "Rendben, Erős";
  String interventionText = "Rendben";
  String boxClass = "b-grn";
  
  String box2Class = "b-grn";
  String box1Class = "b-grn";
  String broodUpClass = "b-grn";
  String broodLowClass = "b-grn";

  if (hiveId == "A1B2") {
    famStatus = "Rendben, Erős";
    interventionText = "Rendben";
    boxClass = "b-grn";
  } else if (hiveId == "B3C4") {
    famStatus = "Fejlesztés alatt";
    interventionText = "5 nap múlva";
    boxClass = "b-yell";
    box2Class = "b-yell";
  } else if (hiveId == "C5D6") {
    famStatus = "Ellenőrzés szükséges";
    interventionText = "2 nap múlva";
    boxClass = "b-org";
    box2Class = "b-org";
    box1Class = "b-org";
  } else if (hiveId == "D7E8") {
    famStatus = "Etetés esedékes";
    interventionText = "Holnap";
    boxClass = "b-red";
    box2Class = "b-red";
    box1Class = "b-red";
  } else if (hiveId == "E9F0") {
    famStatus = "Atkakezelés";
    interventionText = "Ma (Azonnal)";
    boxClass = "b-purp";
    box2Class = "b-purp";
    box1Class = "b-purp";
  } else if (hiveId == "DEAD") {
    famStatus = "Kritikus probléma";
    interventionText = "🔥 Hans";
    boxClass = "b-flame";
    box2Class = "b-flame";
    box1Class = "b-flame";
    broodUpClass = "b-flame";
    broodLowClass = "b-flame";
  }

  bool isHans = (hiveId == "DEAD");

  String html = htmlHead("Kaptár: " + hiveId, "11");

  html += "<style>"
          "@keyframes flammenwerfer { 0% { opacity: 1; background-color: rgba(255,0,0,0.3); } 50% { opacity: 0.4; background-color: rgba(255,0,0,0.8); } 100% { opacity: 1; background-color: rgba(255,0,0,0.3); } }"
          ".hive-stack { display: flex; flex-direction: column; align-items: center; gap: 4px; padding: 12px; background: #0a0a18; border-radius: 12px; border: 1px solid var(--border); max-width: 150px; margin: 0 auto; }"
          ".box-super { width: 100%; display: flex; align-items: center; justify-content: center; font-weight: bold; font-size: 11px; border-radius: 4px; box-shadow: 0 2px 4px rgba(0,0,0,0.4); text-align: center; padding: 0 2px; overflow: hidden; }"
          ".box-square { aspect-ratio: 1 / 1; }"
          ".box-ratio-23 { aspect-ratio: 3 / 2; }"
          ".alert-banner { padding: 12px; border-radius: 8px; font-weight: bold; text-align: center; margin-bottom: 16px; font-size: 15px; }"
          ".hans-meme  { width: 100%; max-height: 200px; object-fit: cover; border-radius: 8px; margin-bottom: 14px; border: 2px solid #ef4444; }"
          
          /* Biztosított háttérszínek a dobozokhoz */
          ".b-grn   { background: rgba(34,197,94,0.3); color: #22c55e; border: 2px solid #22c55e; }"
          ".b-yell  { background: rgba(234,179,8,0.3); color: #eab308; border: 2px solid #eab308; }"
          ".b-org   { background: rgba(249,115,22,0.3); color: #f97316; border: 2px solid #f97316; }"
          ".b-red   { background: rgba(239,68,68,0.3); color: #ef4444; border: 2px solid #ef4444; }"
          ".b-purp  { background: rgba(168,85,247,0.3); color: #a855f7; border: 2px solid #a855f7; }"
          ".b-flame { background: rgba(255,0,0,0.6); color: #fff; border: 2px solid #ff3333; animation: flammenwerfer 0.8s infinite; }"
          "</style>";

  html += "<div style='display:flex; flex-wrap:wrap; justify-content:space-between; align-items:center; width:100%; margin-bottom:15px; gap:10px;'>";
  html += "<div style='display:flex; align-items:center; gap:10px; width:100%;'>";
  html += "<h2 style='margin:0;'>Kaptár:</h2>";
  html += "<select onchange=\"location.href='/hive?hive='+this.value\" style='flex:1; padding:10px; border-radius:8px; background:#141428; color:var(--accent); border:1px solid var(--border); font-size:18px; font-weight:bold; cursor:pointer;'>";
  html += "<option value='A1B2'" + String(hiveId=="A1B2"?" selected":"") + ">A1B2</option>";
  html += "<option value='B3C4'" + String(hiveId=="B3C4"?" selected":"") + ">B3C4</option>";
  html += "<option value='C5D6'" + String(hiveId=="C5D6"?" selected":"") + ">C5D6</option>";
  html += "<option value='D7E8'" + String(hiveId=="D7E8"?" selected":"") + ">D7E8</option>";
  html += "<option value='E9F0'" + String(hiveId=="E9F0"?" selected":"") + ">E9F0</option>";
  html += "<option value='DEAD'" + String(hiveId=="DEAD"?" selected":"") + ">DEAD (Teszt)</option>";
  html += "</select>";
  html += "</div>";
  html += "<a href='/hives' style='width:100%;'><button class='sec' style='width:100%; padding:12px; font-size:15px;'>🗺 Vissza a Térképre</button></a>";
  html += "</div>";

  html += "<div style='display:grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 16px; width:100%;'>";

  // Bal oszlop
  html += "<div style='display:flex; flex-direction:column; gap:16px;'>";

  if (isHans) {
    html += "<div class='alert-banner " + boxClass + "'>🔥 HANS KRITIKUS ÁLLAPOT! 🔥</div>";
    html += "<img src='hans.png' class='hans-meme' alt='Hans'>";
  } else {
    html += "<div class='alert-banner " + boxClass + "'>Státusz: " + interventionText + "</div>";
  }

  html += "<div class='card full' style='margin:0;'>";
  html += "<h2>🐝 Család Adatok</h2>";
  html += stateRow("👑 Anya évjárat", String(queenYear), "y");
  html += stateRow("Család Állapota", famStatus, "");
  html += "</div>";

  html += "<div class='card full' style='margin:0;'>";
  html += "<h2>📡 Telemetria</h2>";
  html += stateRow("Akku", String(batPct) + "%", batPct > 20 ? "g" : "r");
  html += stateRow("Monitor", monStat, "g");
  html += "</div>";

  html += "<div style='display:flex; gap:10px;'>";
  html += "<button class='warn' style='flex:1; padding:14px; font-size:16px;' onclick=\"location.href='/evaluate?hive=" + hiveId + "'\">📝 Kezelés</button>";
  html += "<button class='sec' style='flex:1; padding:14px; font-size:16px;' onclick=\"location.href='/config?hive=" + hiveId + "'\">⚙️ Konfig</button>";
  html += "</div>";

  html += "</div>"; 

  // Jobb oszlop: Kaptár állapot
  html += "<div class='card full' style='margin:0;'>";
  html += "<h2>📦 Kaptár Állapot</h2>";
  
  html += "<div class='hive-stack'>";
  html += "<div class='box-super box-ratio-23 " + box2Class + "'>Méztér 2</div>";
  html += "<div class='box-super box-ratio-23 " + box1Class + "'>Méztér 1</div>";
  html += "<div class='box-super box-square " + broodUpClass + "'>Fészek (Felső)</div>";
  html += "<div class='box-super box-square " + broodLowClass + "'>Fészek (Alsó)</div>";
  html += "<div style='width:100%; height:10px; background:#444; border-radius:2px; margin-top:3px;'></div>";
  html += "</div>";
  html += "</div>"; 

  html += "</div>"; 

  html += htmlFoot();
  server.send(200, "text/html", html);
}

// Segédfüggvények a Linker hibák elkerülésére
void handleNfc() {
  if (!checkPinGuard()) return;
  String html = htmlHead("NFC Olvasás", "1");
  
  String nfcSvg = "<svg viewBox='0 0 64 64' width='48' height='48' stroke='currentColor' fill='none' style='display:block; margin:0 auto 10px auto;'>"
                  "<path d='M14 22 C 24 14, 40 14, 50 22' stroke-width='5' stroke-linecap='round'/>"
                  "<path d='M20 30 C 26 24, 38 24, 44 30' stroke-width='5' stroke-linecap='round'/>"
                  "<path d='M26 38 C 29 35, 35 35, 38 38' stroke-width='4' stroke-linecap='round'/>"
                  "<text x='32' y='57' font-family='sans-serif' font-weight='bold' font-size='16' text-anchor='middle' fill='currentColor' stroke='none'>NFC</text>"
                  "</svg>";

  html += "<div class='card wide'><h2>📱 NFC / RFID Olvasás</h2>";
  html += "<p class='hint'>Kérlek, érints a leolvasóhoz egy NFC kártyát vagy kaptár címkét...</p>";
  html += "<div style='text-align:center; padding:30px; color:var(--accent);'>"
          + nfcSvg +
          "<p style='font-weight:bold; margin-top:10px;'>Várakozás NFC jelre...</p>"
          "</div>";
  html += "<button class='sec' style='width:100%; margin-top:15px;' onclick=\"location.href='/'\">⬅ Vissza a Főoldalra</button>";
  html += "</div>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleMapStatusApi() {
  String json = "{";
  json += "\"signal\": -75,";
  json += "\"fix\": true,";
  json += "\"sat\": 8,";
  json += "\"uptime\": " + String(millis() / 60000) + ",";
  json += "\"heap\": " + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"battery\": 3.95,";
  json += "\"windSpeed\": 12.0,";
  json += "\"operator\": \"Telekom HU\",";
  json += "\"lat\": 47.514600,";
  json += "\"lon\": 19.043500";
  json += "}";
  server.send(200, "application/json", json);
}

void handleEvaluate() {
  if (!checkPinGuard()) return;
  server.send(200, "text/html", htmlHead("Értékelés", "9") + "<div class='card wide'><h2>Kezelés / Értékelés rögzítése</h2><p>Még fejlesztés alatt.</p></div>" + htmlFoot());
}

void handleEvaluatePost() {
  server.sendHeader("Location", "/hives", true);
  server.send(302, "text/plain", "");
}

void handleConfig() {
  if (!checkPinGuard()) return;
  server.send(200, "text/html", htmlHead("Konfig", "4") + "<div class='card wide'><h2>Kaptár Konfiguráció</h2><p>Még fejlesztés alatt.</p></div>" + htmlFoot());
}

void handleConfigPost() {
  server.sendHeader("Location", "/hives", true);
  server.send(302, "text/plain", "");
}

void handleRegisterPart() {
  if (!checkPinGuard()) return;
  server.send(200, "text/html", htmlHead("Alkatrész", "4") + "<div class='card wide'><h2>Alkatrész Regisztráció</h2><p>Még fejlesztés alatt.</p></div>" + htmlFoot());
}

void handleRegisterPartPost() {
  server.sendHeader("Location", "/hives", true);
  server.send(302, "text/plain", "");
}