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
          ".hans-meme-container { width: 100%; text-align: center; margin-bottom: 14px; }"
          ".hans-meme  { width: 100%; max-width: 380px; height: auto; max-height: 200px; object-fit: contain; border-radius: 8px; border: 2px solid #ef4444; display: inline-block; }"
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

  html += "<div style='display:flex; flex-direction:column; gap:16px;'>";

  if (isHans) {
    html += "<div class='alert-banner " + boxClass + "'>🔥 HANS KRITIKUS ÁLLAPOT! 🔥</div>";
    html += "<div class='hans-meme-container'><img src='hans.png' class='hans-meme' alt='Hans'></div>";
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

  html += "<div style='display:flex; gap:10px; margin-bottom:10px;'>";
  html += "<button class='warn' style='flex:1; padding:14px; font-size:16px;' onclick=\"location.href='/treatment?hive=" + hiveId + "'\">📝 Kezelés</button>";
  html += "<button class='warn' style='flex:1; padding:14px; font-size:16px; background:rgba(34,197,94,0.2); border-color:#22c55e; color:#22c55e;' onclick=\"location.href='/evaluation?hive=" + hiveId + "'\">📊 Értékelés</button>";
  html += "</div>";
  html += "<button class='sec' style='width:100%; padding:14px; font-size:16px;' onclick=\"location.href='/config?hive=" + hiveId + "'\">⚙️ Konfig</button>";

  html += "</div>"; 

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
  if (!checkPinGuard()) return;

  String json = "{";
  // Telemetria adatok a jobb/bal felső kártyához
  json += "\"signal\":-68,";
  json += "\"fix\":true,";
  json += "\"sat\":\"5 (3D)\",";
  json += "\"uptime\":120,";
  json += "\"heap\":150,";

  // Kaptárak és Készletek tömbje a térképhez
  json += "\"markers\":[";
  json += "{\"id\":\"A1B2\",\"type\":\"hive\",\"lat\":47.514700,\"lng\":19.043600,\"status\":\"ok\"},";
  json += "{\"id\":\"B3C4\",\"type\":\"hive\",\"lat\":47.514650,\"lng\":19.035500,\"status\":\"ok\"},";
  
  // --- ITATÓk (Water) - 2 db (Jó és Kritikus) ---
  json += "{\"id\":\"W1A2\",\"type\":\"water\",\"lat\":47.514800,\"lng\":19.044000,\"level\":95,\"status\":\"good\"},";
  json += "{\"id\":\"W2B3\",\"type\":\"water\",\"lat\":47.514400,\"lng\":19.042000,\"level\":12,\"status\":\"critical\"},";

  // --- ETETŐK (Syrup) - 2 db (Jó és Alacsony) ---
  json += "{\"id\":\"S3B4\",\"type\":\"syrup\",\"lat\":47.514300,\"lng\":19.044500,\"level\":80,\"status\":\"good\"},";
  json += "{\"id\":\"S4C5\",\"type\":\"syrup\",\"lat\":47.514900,\"lng\":19.041500,\"level\":25,\"status\":\"low\"}";
  json += "]";

  json += "}";
  server.send(200, "application/json", json);
}

void handleTreatment() {
  if (!checkPinGuard()) return;
  String hiveId = server.hasArg("hive") ? server.arg("hive") : "A1B2";

  String html = htmlHead("Kezelés rögzítése: " + hiveId, "11");

  html += "<style>"
          ".treatment-container { max-width: 600px; margin: 0 auto; }"
          ".cat-btn { display: block; width: 100%; padding: 18px; margin-bottom: 12px; font-size: 18px; font-weight: bold; text-align: left; border-radius: 12px; cursor: pointer; background: var(--card); color: var(--txt); border: 2px solid var(--border); transition: 0.2s; }"
          ".cat-btn:hover { background: var(--border); border-color: var(--accent); }"
          ".sub-list { display: none; padding: 8px 0 12px 15px; margin-bottom: 12px; border-left: 3px solid var(--accent); background: rgba(255,255,255,0.02); border-radius: 0 8px 8px 0; }"
          ".sub-btn { display: block; width: 100%; padding: 14px; margin-top: 8px; font-size: 16px; font-weight: bold; text-align: left; border-radius: 8px; cursor: pointer; background: #141428; color: var(--txt); border: 1px solid var(--border); }"
          ".sub-btn:hover { background: var(--border); border-color: var(--accent); }"
          "</style>";

  html += "<div class='card wide treatment-container'>";
  html += "<h2>📝 Kezelés rögzítése - Kaptár: " + hiveId + "</h2>";
  html += "<p class='hint'>Válaszd ki a kategóriát (kesztyűbarát mód):</p>";

  html += "<script>"
          "function toggleCat(id) {"
          "  let el = document.getElementById(id);"
          "  let all = document.querySelectorAll('.sub-list');"
          "  all.forEach(s => { if(s.id !== id) s.style.display = 'none'; });"
          "  el.style.display = (el.style.display === 'block') ? 'none' : 'block';"
          "}"
          "</script>";

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-feeding')\">🍯 Etetés ▾</button>";
  html += "<div id='cat-feeding' class='sub-list'>";
  html += "<button class='sub-btn' onclick=\"let v=prompt('Szirup mennyisége (l):','1'); if(v) { alert('Mentve: Szirup - ' + v + ' l'); location.href='/hive?hive=" + hiveId + "'; }\">🐝 Szirup (l)</button>";
  html += "<button class='sub-btn' onclick=\"let v=prompt('Cukorlepény mennyisége (kg):','1'); if(v) { alert('Mentve: Cukorlepény - ' + v + ' kg'); location.href='/hive?hive=" + hiveId + "'; }\">🐝 Cukorlepény (kg)</button>";
  html += "<button class='sub-btn' onclick=\"let v=prompt('Fehérjés lepény mennyisége (kg):','1'); if(v) { alert('Mentve: Fehérjés lepény - ' + v + ' kg'); location.href='/hive?hive=" + hiveId + "'; }\">🐝 Fehérjés lepény (kg)</button>";
  html += "<button class='sub-btn' onclick=\"let v=prompt('Gyógyszeres lepény mennyisége (kg):','1'); if(v) { alert('Mentve: Gyógyszeres lepény - ' + v + ' kg'); location.href='/hive?hive=" + hiveId + "'; }\">🐝 Gyógyszeres lepény (kg)</button>";
  html += "</div>";

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-medical')\">💊 Gyógykezelés ▾</button>";
  html += "<div id='cat-medical' class='sub-list'>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Nosevit'); location.href='/hive?hive=" + hiveId + "';\">🐝 Nosevit</button>";
  html += "</div>";

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-mites')\">🛡️ Atkairtás ▾</button>";
  html += "<div id='cat-mites' class='sub-list'>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Oxálsav - Tartós hordozó'); location.href='/hive?hive=" + hiveId + "';\">↳ Oxálsav: Tartós hordozó</button>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Oxálsav - Spriccelés'); location.href='/hive?hive=" + hiveId + "';\">↳ Oxálsav: Spriccelés</button>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Oxálsav - Szublimálás'); location.href='/hive?hive=" + hiveId + "';\">↳ Oxálsav: Szublimálás</button>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Hangyasav'); location.href='/hive?hive=" + hiveId + "';\">🐝 Hangyasav</button>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Illóolaj'); location.href='/hive?hive=" + hiveId + "';\">🐝 Egyéb: Illóolaj</button>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Heresejt-kivágás'); location.href='/hive?hive=" + hiveId + "';\">🐝 Egyéb: Heresejt-kivágás</button>";
  html += "</div>";

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-other')\">⚙️ Egyéb / Beavatkozások ▾</button>";
  html += "<div id='cat-other' class='sub-list'>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Anyacsere'); location.href='/hive?hive=" + hiveId + "';\">🐝 Anyacsere</button>";
  html += "<button class='sub-btn' onclick=\"alert('Mentve: Család felszámolása'); location.href='/hive?hive=" + hiveId + "';\">🐝 Család felszámolása</button>";
  html += "<button class='sub-btn' style='border-color:#ef4444; color:#ef4444;' onclick=\"if(confirm('🔥 BIZTOSAN végrehajtod a Hans protokollt?')) { alert('🔥 Hans protokoll végrehajtva!'); location.href='/hive?hive=" + hiveId + "'; }\">🔥 Hans (Biztos igen/nem)</button>";
  html += "</div>";

  html += "<button class='sec' style='margin-top:20px; padding:16px; font-size:16px; width:100%;' onclick=\"location.href='/hive?hive=" + hiveId + "'\">⬅ Vissza a kaptárhoz</button>";
  html += "</div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
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

void handleGetTreatmentsJson() {
  Serial.println("[TREATMENTS] /api/treatments vegpont meghivva...");
  if (!LittleFS.exists("/treatment.json")) {
    Serial.println("[TREATMENTS] HIBA: A /treatment.json fajl nem talalhato a LittleFS-en!");
    server.send(404, "application/json", "{\"error\":\"treatment.json not found\"}");
    return;
  }
  
  File f = LittleFS.open("/treatment.json", "r");
  if (!f) {
    Serial.println("[TREATMENTS] HIBA: A fajl megnyitasa olvasasra sikertelen!");
    server.send(500, "application/json", "{\"error\":\"Failed to open file\"}");
    return;
  }
  
  Serial.println("[TREATMENTS] Siker: treatment.json tovabbitasa a kliensnek.");
  server.streamFile(f, "application/json");
  f.close();
}

void handleEvaluatePost() {
  server.sendHeader("Location", "/hives", true);
  server.send(302, "text/plain", "");
}

void handleEvaluation() {
  if (!checkPinGuard()) return;
  String hiveId = server.hasArg("hive") ? server.arg("hive") : "A1B2";

  String html = htmlHead("Értékelés: " + hiveId, "11");

  html += "<style>"
          ".eval-container { max-width: 600px; margin: 0 auto; }"
          ".cat-btn { display: block; width: 100%; padding: 18px; margin-bottom: 12px; font-size: 18px; font-weight: bold; text-align: left; border-radius: 12px; cursor: pointer; background: var(--card); color: var(--txt); border: 2px solid var(--border); transition: 0.2s; }"
          ".cat-btn:hover { background: var(--border); border-color: var(--accent); }"
          ".sub-list { display: none; padding: 8px 0 12px 15px; margin-bottom: 12px; border-left: 3px solid var(--accent); background: rgba(255,255,255,0.02); border-radius: 0 8px 8px 0; }"
          ".eval-item { margin-bottom: 20px; }"
          ".eval-label { font-size: 16px; font-weight: bold; margin-bottom: 8px; color: var(--txt); }"
          ".eval-btns { display: flex; gap: 8px; width: 100%; }"
          ".btn-mm { flex: 1; padding: 16px 0; font-size: 22px; font-weight: bold; border-radius: 8px; background: rgba(239,68,68,0.2); border: 2px solid #ef4444; color: #ef4444; cursor: pointer; }"
          ".btn-m  { flex: 1; padding: 16px 0; font-size: 22px; font-weight: bold; border-radius: 8px; background: rgba(249,115,22,0.2); border: 2px solid #f97316; color: #f97316; cursor: pointer; }"
          ".btn-p  { flex: 1; padding: 16px 0; font-size: 22px; font-weight: bold; border-radius: 8px; background: rgba(132,204,22,0.2); border: 2px solid #84cc16; color: #84cc16; cursor: pointer; }"
          ".btn-pp { flex: 1; padding: 16px 0; font-size: 22px; font-weight: bold; border-radius: 8px; background: rgba(34,197,94,0.2); border: 2px solid #22c55e; color: #22c55e; cursor: pointer; }"
          "</style>";

  html += "<div class='card wide eval-container'>";
  html += "<h2>📊 Értékelés - Kaptár: " + hiveId + "</h2>";
  html += "<p class='hint'>Válaszd ki a kategóriát, majd az értéket (kesztyűbarát mód):</p>";

  html += "<script>"
          "function toggleCat(id) {"
          "  let el = document.getElementById(id);"
          "  let all = document.querySelectorAll('.sub-list');"
          "  all.forEach(s => { if(s.id !== id) s.style.display = 'none'; });"
          "  el.style.display = (el.style.display === 'block') ? 'none' : 'block';"
          "}"
          "function saveEval(trait, val) {"
          "  alert('Mentve: ' + trait + ' -> ' + val);"
          "  location.href='/hive?hive=" + hiveId + "';"
          "}"
          "</script>";

  auto evalRow = [](String label) -> String {
      String s = "<div class='eval-item'><div class='eval-label'>" + label + "</div><div class='eval-btns'>";
      s += "<button class='btn-mm' onclick=\"saveEval('" + label + "', '--')\">--</button>";
      s += "<button class='btn-m' onclick=\"saveEval('" + label + "', '-')\">-</button>";
      s += "<button class='btn-p' onclick=\"saveEval('" + label + "', '+')\">+</button>";
      s += "<button class='btn-pp' onclick=\"saveEval('" + label + "', '++')\">++</button>";
      s += "</div></div>";
      return s;
  };

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-behavior')\">🐝 Viselkedés ▾</button>";
  html += "<div id='cat-behavior' class='sub-list'>";
  html += evalRow("Szelíd");
  html += evalRow("Tisztító");
  html += evalRow("Építő");
  html += evalRow("Takarékos");
  html += "</div>";

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-collection')\">🌼 Gyűjtés ▾</button>";
  html += "<div id='cat-collection' class='sub-list'>";
  html += evalRow("Méz");
  html += evalRow("Virágpor");
  html += evalRow("Propolisz");
  html += "</div>";

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-tendency')\">⚠️ Hajlamok ▾</button>";
  html += "<div id='cat-tendency' class='sub-list'>";
  html += evalRow("Rajzási");
  html += evalRow("Rablási");
  html += "</div>";

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-health')\">🛡️ Egészség ▾</button>";
  html += "<div id='cat-health' class='sub-list'>";
  html += evalRow("Betegség ellenállás");
  html += evalRow("Atka ellenállás");
  html += "</div>";

  html += "<button class='cat-btn' onclick=\"toggleCat('cat-queen')\">👑 Anya ▾</button>";
  html += "<div id='cat-queen' class='sub-list'>";
  html += evalRow("Fiasítás mennyisége");
  html += evalRow("Fiasítás zártsága");
  html += evalRow("Megjelenés");
  html += "</div>";

  html += "<button class='sec' style='margin-top:20px; padding:16px; font-size:16px; width:100%;' onclick=\"location.href='/hive?hive=" + hiveId + "'\">⬅ Vissza a kaptárhoz</button>";
  html += "</div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}