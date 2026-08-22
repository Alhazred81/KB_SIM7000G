// web_config.cpp

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "web_config.h"
#include "web_common.h"
#include "NtfyClient.h"
#include "wifi_sta.h" // A kliens hálózatkezeléshez

extern WebServer server;
extern bool checkPinGuard();
extern NtfyClient ntfy;

extern String gApSSID;
extern int gApChannel;
extern String gNtfyServer;
extern String gNtfyTopic;
extern String gNtfyNickname;
extern bool gNtfyStartupMsg;

void handleCfg() {
  if (!checkPinGuard()) return;
  String html = htmlHead("Beallitasok", "4");

  // --- 1. WiFi Kliens (STA) csatlakozás rész ---
  html += "<div class='card wide'><h2>WiFi Hálózatra Csatlakozás (Kliens mód)</h2>";
  html += "<p class='hint'>Ha megadsz egy WiFi hálózatot, a szerver megpróbál csatlakozni hozzá az AP mód mellett, így elérheti az internetet GSM nélkül is.</p>";
  
  String staStatus = (gSta.mode == NetMode::STA_CONNECTED) ? "Csatlakozva" : 
                     (gSta.mode == NetMode::STA_CONNECTING) ? "Csatlakozás folyamatban..." : "Nincs csatlakozva";
  String staColor = (gSta.mode == NetMode::STA_CONNECTED) ? "g" : (gSta.mode == NetMode::STA_CONNECTING ? "y" : "r");
  
  html += stateRow("Állapot", staStatus, staColor);
  
  if(gSta.mode == NetMode::STA_CONNECTED) {
    html += stateRow("SSID", gSta.targetSSID, "");
    html += stateRow("IP cím", gSta.ip, "");
    html += "<form action='/stadisconnect' method='POST' style='margin-top:10px'>";
    html += "<button class='danger'>Lecsatlakozás</button></form>";
  } else {
    if(gSta.lastError.length()) {
      html += "<div class='msg err'>" + htmlEscape(gSta.lastError) + "</div>";
    }
    
    // Ha a keresőből jövünk vissza, automatikusan kitölti az SSID-t
    String autoFillSsid = server.hasArg("set_ssid") ? server.arg("set_ssid") : gSta.targetSSID;
    
    html += "<form action='/staconnect' method='POST' style='margin-top:10px;'>";
    html += "<label>SSID (Hálózat neve)</label>";
    html += "<input type='text' name='sta_ssid' value='" + htmlEscape(autoFillSsid) + "'>";
    html += "<label>Jelszó</label>";
    html += "<input type='password' name='sta_pass' placeholder='Hagyd üresen, ha nyílt a hálózat'>";
    html += "<div style='display:flex;gap:10px;margin-top:10px;'>";
    html += "<button type='submit' style='flex:2;'>Csatlakozás</button>";
    html += "<button type='button' class='sec' onclick='location.href=\"/wifiscan\"' style='flex:1;'>Keresés</button>";
    html += "</div></form>";
  }
  html += "</div>";

  // --- 2. WiFi AP beállítások ---
  html += "<div class='card wide'><h2>WiFi AP (Saját hálózat)</h2>"
          "<form action='/savewifi' method='POST'>"
          "<label>SSID vege (elotag: KB-teszt-)</label>"
          "<input type='text' name='ssid' value='";
  String macPart = gApSSID.length()>9 ? gApSSID.substring(9) : "";
  html += macPart;
  html += "' maxlength='20'>"
          "<label>Jelszo (min. 8 kar.)</label>"
          "<input type='password' name='pass' value='' placeholder='ures = valtozatlan' maxlength='31'>"
          "<label>Csatorna</label>"
          "<select name='ch'>";
  for(int i=1; i<=13; i++){
    html += "<option value='" + String(i) + "'" + (i==gApChannel ? " selected" : "") + ">Csatorna " + String(i) + "</option>";
  }
  html += "</select><button>Mentes & ujraindulas</button></form></div>";

  // --- 3. Ntfy Beállítások ---
  html += "<div class='card wide'><h2>ntfy Beállítások (Üzenetcsatorna)</h2>"
          "<form action='/save-ntfy' method='POST'>"
          "<label>ntfy Szerver</label>"
          "<input type='text' name='ntfy_server' value='" + htmlEscape(gNtfyServer) + "'>"
          "<label>Topic neve (egyedi azonosító)</label>"
          "<input type='text' name='ntfy_topic' value='" + htmlEscape(gNtfyTopic) + "' required>"
          "<label>Eszközazonosító (Név, pl. szerver-1)</label>"
          "<input type='text' name='ntfy_nickname' value='" + htmlEscape(gNtfyNickname) + "'>"
          
          "<div style='display:flex; align-items:center; justify-content:space-between; margin-top:15px; padding-top:10px; border-top:1px solid var(--border);'>"
          "<span>Rendszerindulási tesztüzenet</span>"
          "<label class='sens-toggle' style='--sens-color:var(--ok); margin:0;'>"
          "<input type='checkbox' name='ntfy_startup'" + String(gNtfyStartupMsg ? " checked" : "") + ">"
          "<span class='slider'></span></label>"
          "</div>"
          
          "<button style='margin-top:20px'>ntfy Mentés</button>"
          "</form></div>";

  // --- 4. Időjárás és Vihar Riasztás Beállítások ---
  Preferences prefsW;
  prefsW.begin("weather_cfg", true);
  bool wDebug = prefsW.getBool("w_debug", false);
  float wRain = prefsW.getFloat("w_rain", 5.0);
  int wWind = prefsW.getInt("w_wind", 45);
  int wPrio  = prefsW.getInt("w_prio", 5);
  prefsW.end();

  html += "<div class='card wide'><h2>Időjárás & Vihar Riasztás Beállítások</h2>"
          "<form action='/saveweathercfg' method='POST'>"
          
          "<div style='display:flex; align-items:center; justify-content:space-between; margin-bottom:15px; padding-top:10px; border-top:1px solid var(--border);'>"
          "<span>YR/Meteo JSON nyers kiírás a terminálra</span>"
          "<label class='sens-toggle' style='--sens-color:var(--prim); margin:0;'>"
          "<input type='checkbox' name='w_debug'" + String(wDebug ? " checked" : "") + ">"
          "<span class='slider'></span></label>"
          "</div>"

          "<label>Esőintenzitás küszöb riasztáshoz (mm/h)</label>"
          "<input type='number' step='0.5' name='w_rain' value='" + String(wRain, 1) + "' style='width:100%; margin-bottom:15px;'>"

          "<label>Szélerősség küszöb riasztáshoz (km/h)</label>"
          "<input type='number' name='w_wind' value='" + String(wWind) + "' style='width:100%; margin-bottom:15px;'>"

          "<label>Riasztási ntfy prioritás (1 - alacsony, 5 - vészhelyzet)</label>"
          "<select name='w_prio' style='width:100%; margin-bottom:20px; padding:8px; background:#0a0a18; color:var(--txt); border:1px solid var(--border); border-radius:6px;'>"
          "<option value='1'" + String(wPrio == 1 ? " selected" : "") + ">1 - Min (Alacsony)</option>"
          "<option value='2'" + String(wPrio == 2 ? " selected" : "") + ">2 - Low</option>"
          "<option value='3'" + String(wPrio == 3 ? " selected" : "") + ">3 - Default (Normál)</option>"
          "<option value='4'" + String(wPrio == 4 ? " selected" : "") + ">4 - High (Magas)</option>"
          "<option value='5'" + String(wPrio == 5 ? " selected" : "") + ">5 - Urgent (Vészhelyzet)</option>"
          "</select>"

          "<button>Időjárás Beállítások Mentése</button>"
          "</form>"

          "<hr style='border:0; border-top:1px solid var(--border); margin:20px 0;'>"
          "<button type='button' class='sec' onclick='sendWeatherTest()' id='testAlertBtn' style='width:100%;'>⚡ Vihar Riasztás Tesztküldése</button>"
          "<div id='testAlertRes' class='msg' style='display:none; margin-top:10px;'></div>"
          
          "<script>"
          "function sendWeatherTest() {"
          "  var btn = document.getElementById('testAlertBtn');"
          "  var res = document.getElementById('testAlertRes');"
          "  btn.disabled = true; btn.innerText = 'Küldés...';"
          "  fetch('/testweatheralert', {method: 'POST'})"
          "    .then(r => r.text())"
          "    .then(txt => {"
          "      res.style.display = 'block';"
          "      if(txt === 'ok') {"
          "        res.className = 'msg ok'; res.innerText = 'Teszt riasztás sikeresen elküldve ntfy-on!';"
          "      } else {"
          "        res.className = 'msg err'; res.innerText = 'Hiba a küldéskor: ' + txt;"
          "      }"
          "      btn.disabled = false; btn.innerText = '⚡ Vihar Riasztás Tesztküldése';"
          "    }).catch(err => {"
          "      res.style.display = 'block'; res.className = 'msg err'; res.innerText = 'Hálózati hiba.';"
          "      btn.disabled = false; btn.innerText = '⚡ Vihar Riasztás Tesztküldése';"
          "    });"
          "}"
          "</script>"
          "</div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

// --- WiFi Kliens / Hálózatkereső Handler Függvények ---

void handleWifiScan() {
  if (!checkPinGuard()) return;
  wifiScan(); // Blokkol néhány másodpercig, amíg keres
  
  String html = htmlHead("WiFi Keresés", "4");
  html += "<div class='card wide'><h2>Elérhető WiFi hálózatok</h2>";
  
  if (gScanCount == 0) {
    html += "<div class='msg err'>Nem található egyetlen hálózat sem.</div>";
  } else {
    html += "<p class='hint'>Kattints a Kiválaszt gombra a csatlakozáshoz.</p>";
    for (int i = 0; i < gScanCount; i++) {
      html += "<div style='display:flex; justify-content:space-between; align-items:center; border-bottom:1px solid var(--border); padding:8px 0;'>";
      html += "<div><b>" + htmlEscape(gScanResults[i].ssid) + "</b><br>";
      html += "<span class='hint' style='margin:0'>Jelerősség: " + String(gScanResults[i].rssi) + " dBm | " + (gScanResults[i].secure ? "🔒 Védett" : "🔓 Nyílt") + "</span></div>";
      html += "<form action='/cfg' method='GET' style='margin:0;'>";
      html += "<input type='hidden' name='set_ssid' value='" + htmlEscape(gScanResults[i].ssid) + "'>";
      html += "<button class='sec' style='width:auto; padding:6px 12px;'>Kiválaszt</button></form>";
      html += "</div>";
    }
  }
  
  html += "<div style='display:flex; gap:10px; margin-top:20px;'>";
  html += "<form action='/wifiscan' method='GET' style='flex:1;'><button>🔄 Újra keresés</button></form>";
  html += "<form action='/cfg' method='GET' style='flex:1;'><button type='button' class='sec' onclick='location.href=\"/cfg\"'>Vissza</button></form>";
  html += "</div></div>";
  
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleStaConnect() {
  if (server.hasArg("sta_ssid")) {
    wifiStaConnect(server.arg("sta_ssid"), server.arg("sta_pass"));
  }
  server.sendHeader("Location", "/cfg", true);
  server.send(302, "text/plain", "");
}

void handleStaDisconnect() {
  wifiStaDisconnect();
  server.sendHeader("Location", "/cfg", true);
  server.send(302, "text/plain", "");
}

// --- Mentési Handler-ek (Változatlanok) ---

void handleSaveWeatherCfg() {
  Preferences prefsW;
  prefsW.begin("weather_cfg", false);
  prefsW.putBool("w_debug", server.hasArg("w_debug"));
  if (server.hasArg("w_rain")) prefsW.putFloat("w_rain", server.arg("w_rain").toFloat());
  if (server.hasArg("w_wind")) prefsW.putInt("w_wind", server.arg("w_wind").toInt());
  if (server.hasArg("w_prio")) prefsW.putInt("w_prio", server.arg("w_prio").toInt());
  prefsW.end();

  server.sendHeader("Location", "/cfg", true);
  server.send(302, "text/plain", "");
}

void handleSaveWifi() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    gApSSID = "KB-teszt-" + server.arg("ssid");
  }
  server.sendHeader("Location", "/cfg", true);
  server.send(302, "text/plain", "");
}

void handleTestWeatherAlert() {
  Preferences prefsW;
  prefsW.begin("weather_cfg", true);
  int prio = prefsW.getInt("w_prio", 5);
  prefsW.end();

  NtfyPriority ntfyPrio = static_cast<NtfyPriority>(prio);
  bool success = ntfy.send("Ez egy teszt vihar riasztas.", "Vihar Riasztas Teszt", ntfyPrio);
  
  if (success) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(500, "text/plain", "ntfy küldés sikertelen");
  }
}