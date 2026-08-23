#include "web_supply.h"
#include <WebServer.h>
#include "web_common.h"

extern WebServer server;

void handleSupply() {
  if (!checkPinGuard()) return;

  String html = htmlHead("Készletek", "3");

  html += "<style>"
          ".supply-card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 16px; margin-bottom: 16px; box-shadow: 0 4px 6px rgba(0,0,0,0.2); }"
          ".supply-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }"
          ".supply-title { font-size: 18px; font-weight: bold; color: var(--txt); display: flex; align-items: center; gap: 8px; }"
          ".supply-status { font-size: 12px; color: var(--txt2); }"
          ".prog-bg { width: 100%; height: 28px; background: rgba(255,255,255,0.1); border-radius: 14px; overflow: hidden; position: relative; margin: 10px 0; border: 1px solid rgba(255,255,255,0.05); }"
          ".prog-bar { height: 100%; display: flex; align-items: center; justify-content: center; font-size: 14px; font-weight: bold; color: #fff; text-shadow: 1px 1px 2px rgba(0,0,0,0.5); transition: width 0.5s ease; }"
          
          /* Színátmenetek a különböző folyadékszintekhez */
          ".bar-water { background: linear-gradient(90deg, #2563eb, #3b82f6); }"
          ".bar-syrup { background: linear-gradient(90deg, #ca8a04, #eab308); }"
          ".bar-low   { background: linear-gradient(90deg, #ea580c, #f97316); }" /* Narancssárga riasztás */
          ".bar-crit  { background: linear-gradient(90deg, #dc2626, #ef4444); }" /* Piros kritikus */
          
          ".info-row { display: flex; justify-content: space-between; font-size: 14px; margin-bottom: 6px; }"
          ".btn-edit { background: rgba(255,255,255,0.1); border: 1px solid var(--border); color: var(--txt); padding: 12px; width: 100%; border-radius: 8px; font-size: 16px; font-weight: bold; cursor: pointer; margin-top: 10px; transition: 0.2s; }"
          ".btn-edit:hover { background: var(--border); }"
          "</style>";

  html += "<h2 style='margin-bottom: 16px;'>💧 Itatók és 🍯 Etetők</h2>";

  // --- Segédfüggvény a szintek és színek kiszámításához ---
  auto getLevelInfo = [](int level, bool isWater, String& outClass, String& outText) {
    if (level <= 15) {
      outClass = "bar-crit";
      outText = "Kritikus (" + String(level) + "%)";
    } else if (level <= 30) {
      outClass = "bar-low";
      outText = "Alacsony (" + String(level) + "%)";
    } else if (level >= 90) {
      outClass = isWater ? "bar-water" : "bar-syrup";
      outText = "Maximum (" + String(level) + "%)";
    } else {
      outClass = isWater ? "bar-water" : "bar-syrup";
      outText = "Normál (" + String(level) + "%)";
    }
  };

  // ==========================================
  // 1. KÁRTYA: ITATÓ (Tegyük fel, hogy szinte tele van)
  // ==========================================
  int waterLevel = 95; // Példa adat
  String waterClass, waterText;
  getLevelInfo(waterLevel, true, waterClass, waterText);
  
  html += "<div class='supply-card'>";
  html += "<div class='supply-header'>";
  html += "<div class='supply-title'>💧 Fő Itató (ID: W1A2)</div>";
  html += "<div class='supply-status'>🟢 Aktív (3 perce)</div>";
  html += "</div>";
  
  html += "<div class='info-row'><span>Akku: <b>3.9V</b></span> <span>Dőlés: <b>OK</b></span></div>";
  
  html += "<div class='prog-bg'>";
  html += "<div class='prog-bar " + waterClass + "' style='width: " + String(waterLevel) + "%;'>" + waterText + "</div>";
  html += "</div>";
  html += "<div class='info-row'><span style='color:var(--txt2); font-size:12px;'>Szenzor táv.: 22 cm</span> <span style='font-weight:bold;'>Kb. 45 Liter</span></div>";
  html += "</div>";

  // ==========================================
  // 2. KÁRTYA: SZIRUPADAGOLÓ (Tegyük fel, hogy kritikus)
  // ==========================================
  int syrupLevel = 12; // Példa adat
  String syrupClass, syrupText;
  getLevelInfo(syrupLevel, false, syrupClass, syrupText);
  
  String currentMix = "1:1 Cukorszirup + Nosevit"; 
  
  html += "<div class='supply-card'>";
  html += "<div class='supply-header'>";
  html += "<div class='supply-title'>🍯 1. Sor Etető (ID: S3B4)</div>";
  html += "<div class='supply-status'>🟢 Aktív (1 perce)</div>";
  html += "</div>";
  
  html += "<div class='info-row'><span>Akku: <b>4.1V</b></span> <span>Dőlés: <b style='color:#ef4444;'>RIASZTÁS!</b></span></div>";
  
  html += "<div class='prog-bg'>";
  // Mivel 12% túl keskeny lehet a szöveghez, rögzítjük a minimum szélességet, hogy kiférjen a felirat
  int barWidth = (syrupLevel < 35) ? 35 : syrupLevel; 
  html += "<div class='prog-bar " + syrupClass + "' style='width: " + String(barWidth) + "%;'>" + syrupText + "</div>";
  html += "</div>";
  html += "<div class='info-row'><span style='color:var(--txt2); font-size:12px;'>Tartalom:</span> <span style='font-weight:bold; color:var(--accent);'>" + currentMix + "</span></div>";
  
  html += "<button class='btn-edit' onclick=\"let m=prompt('Mit töltöttél az adagolóba? (pl. 1:1 + Nosevit)','" + currentMix + "'); if(m){ alert('Mentve: '+m); location.reload(); }\">📝 Tartalom módosítása</button>";
  html += "</div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}