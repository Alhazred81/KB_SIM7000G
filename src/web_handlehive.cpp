#include <WebServer.h>
#include <Preferences.h>
#include "web_handlehive.h"
#include "web_common.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

extern WebServer server;
extern bool checkPinGuard();

// Fix sávos színkód generálás
String getInterventionColorStyle(String daysStatus) {
  if (daysStatus == "hans") return "background: rgba(255,0,0,0.5); color: #fff; animation: flammenwerfer 0.8s infinite;";
  if (daysStatus == "0") return "background: rgba(255,0,255,0.2); color: #ff00ff; border-color: #ff00ff;";
  if (daysStatus == "1") return "background: rgba(255,0,0,0.2); color: #ff3333; border-color: #ff3333;";
  if (daysStatus == "3_under") return "background: rgba(255,165,0,0.2); color: #ffa500; border-color: #ffa500;";
  if (daysStatus == "3_over") return "background: rgba(255,255,0,0.2); color: #e6e600; border-color: #e6e600;";
  return "background: rgba(0,204,102,0.2); color: var(--ok);";
}

// Dinamikus stílus generálás a color_coding.json alapján (opcionális)
String getDynamicColorStyle(const String& statusKey) {
  File file = LittleFS.open("/color_coding.json", "r");
  if (!file) return "background: rgba(0,204,102,0.2); color: var(--ok);"; // Fallback zöld

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) return "background: rgba(0,204,102,0.2); color: var(--ok);";

  JsonObject obj = doc[statusKey];
  if (obj.isNull()) {
    // Alapértelmezett, ha nem találja
    obj = doc["later"]; 
  }

  const char* color = obj["color"] | "#ffffff";
  const char* bg = obj["bg"] | "rgba(0,0,0,0.2)";
  bool animate = obj["animate"] | false;

  String style = "background: " + String(bg) + "; color: " + String(color) + "; border-color: " + String(color) + ";";
  if (animate) {
    style += " animation: flammenwerfer 0.8s infinite;";
  }
  return style;
}

void handleHiveView() {
  if (!checkPinGuard()) return;
  
  String hiveId = server.hasArg("hive") ? server.arg("hive") : "Ismeretlen";
  
  int queenYear = 2023;
  String famStatus = "Termelő";
  int batPct = 90;
  String monStat = "OK";
  
  // Teszteléshez állítsd be a megfelelő értékeket
  String interventionStatus = "3_over"; 
  bool isHans = (interventionStatus == "hans");

  // Folyamatban lévő kezelés tesztadatai
  String activeTreatment = "Anyásítás (Zavarásmentes időszak)"; 
  int treatmentDaysLeft = 11; 
  bool isDoNotDisturb = (activeTreatment.indexOf("Anyásítás") >= 0);

  String html = htmlHead("Kaptár Részletek: " + hiveId, "0");

  html += "<style>"
          "@keyframes flammenwerfer { 0% { opacity: 1; background-color: rgba(255,0,0,0.3); } 50% { opacity: 0.4; background-color: rgba(255,0,0,0.8); } 100% { opacity: 1; background-color: rgba(255,0,0,0.3); } }"
          ".hive-dashboard { background: var(--card); border: 1px solid var(--border); border-radius: 10px; padding: 15px; margin-bottom: 15px; }"
          ".hive-header-info { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; font-size: 14px; }"
          ".alert-banner { padding: 10px; border-radius: 6px; font-weight: bold; text-align: center; margin-bottom: 15px; font-size: 14px; " + getInterventionColorStyle(interventionStatus) + " }"
          ".treatment-banner { padding: 10px; border-radius: 6px; font-weight: bold; text-align: center; margin-bottom: 15px; font-size: 14px; background: rgba(0, 150, 255, 0.2); color: #0096ff; border: 1px solid #0096ff; margin-top: -5px; }"
          ".hive-stack { display: flex; flex-direction: column; align-items: center; gap: 4px; padding: 12px; background: rgba(0,0,0,0.15); border-radius: 8px; }"
          ".box-super { width: 90%; display: flex; align-items: center; justify-content: center; font-weight: bold; font-size: 12px; color: #fff; border: 2px solid #2a2a40; border-radius: 4px; }"
          ".box-high   { height: 55px; background: #8B4513; }" 
          ".box-med    { height: 40px; background: #CD853F; }" 
          ".box-flat   { height: 25px; background: #DEB887; }" 
          ".hans-box   { background: rgba(255,0,0,0.6) !important; border-color: #ff0000 !important; animation: flammenwerfer 0.8s infinite; }"
          ".hans-meme  { width: 100%; max-height: 180px; object-fit: cover; border-radius: 6px; margin-bottom: 12px; border: 2px solid #ff3333; }"
          ".dnd-meme   { width: 100%; max-height: 180px; object-fit: cover; border-radius: 6px; margin-bottom: 12px; border: 2px solid #0096ff; }"
          ".box-focus  { border: 2px dashed #FFFF00; box-shadow: 0 0 8px rgba(255,255,0,0.8); }"
          ".action-buttons { display: flex; gap: 10px; margin-top: 15px; }"
          "</style>";

  html += "<h2>Kaptár Részletek: <span style='color:var(--prim)'>" + hiveId + "</span></h2>";

  html += "<div class='hive-dashboard'>";
  
  String alertText = isHans ? "🔥 HANS KRITIKUS ÁLLAPOT!" : "Beavatkozási ütemterv státusz: " + interventionStatus;
  html += "<div class='alert-banner'>" + alertText + "</div>";

  // Hans állapot esetén kép megjelenítése
  if (isHans) {
    html += "<div style='text-align:center;'>";
    html += "<img src='hans.png' class='hans-meme' alt='Hans, get ze flammenwerfer'>";
    html += "</div>";
  }

  // Aktív folyamat és "Ne nyúlj hozzám" mód
  if (activeTreatment != "") {
    html += "<div class='treatment-banner'>";
    html += "⏳ Aktív folyamat: <b>" + activeTreatment + "</b> (" + String(treatmentDaysLeft) + " nap hátra)";
    html += "</div>";

    if (isDoNotDisturb) {
      html += "<div style='text-align:center;'>";
      html += "<img src='donttouch.png' class='dnd-meme' alt='Don\\'t you touch me!'>";
      html += "</div>";
    }
  }

  html += "<div class='hive-header-info'>";
  html += "<span>👑 Anya évjárat: <b>" + String(queenYear) + "</b></span>";
  html += "<span>Státusz: <b>" + famStatus + "</b></span>";
  html += "</div>";

  html += "<div class='hive-header-info'>";
  html += "<span>🔋 Akku: <b>" + String(batPct) + "%</b></span>";
  html += "<span>📡 Monitor: <b style='color:var(--ok)'>" + monStat + "</b></span>";
  html += "</div>";

  String hansClass = isHans ? " hans-box" : "";
  
  html += "<div class='hive-stack'>";
  html += "<div class='box-super box-high box-focus" + hansClass + "'>Fészek (Alsó) 🟢</div>";
  html += "<div class='box-super box-high" + hansClass + "'>Fészek (Felső)</div>";
  html += "<div class='box-super box-flat" + hansClass + "'>Félfiók (Méztér 1) ✨</div>";
  html += "<div class='box-super box-flat" + hansClass + "'>Félfiók (Méztér 2) ✨</div>";
  html += "</div>";

  html += "</div>";

  html += "<div class='action-buttons'>";
  html += "<button class='warn' style='flex:1; padding:12px;' onclick=\"location.href='/evaluate?hive=" + hiveId + "'\">📝 Kezelés</button>";
  html += "<button class='sec' style='flex:1; padding:12px;' onclick=\"location.href='/config?hive=" + hiveId + "'\">⚙️ Konfig</button>";
  html += "</div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}