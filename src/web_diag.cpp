//web_diag.cpp 
#include "web_diag.h"
#include "web_common.h"
#include "modem_mgr.h"
#include "time_mgr.h"
#include <WebServer.h>
#include <WiFi.h> // WiFi.softAPgetStationNum() miatt

extern WebServer server;
extern ModemState gModem;
extern TimeState gTime;
extern String gApSSID;
extern uint8_t gApChannel;
extern bool gModemInitRequested;
extern String gAtStatusSnapshot;
extern unsigned long gAtStatusSnapshotAt;

// Külső függvények deklarációja
extern void refreshAtStatusSnapshot();
extern String modemApplyExpertConfig(const String& cnmp, const String& cgsms, const String& bands, const String& cmnb);
extern void modemResetExpertConfig();
extern void sendWaitPage(const String& title, const String& message, const String& nextUrl, int waitSeconds);

void handleEspRestart() {
  String html = htmlHead("Rendszer Újraindítás", "5");
  html += "<div class='card' style='text-align:center; padding:30px;'>";
  html += "<h2>Az ESP32 újraindul...</h2>";
  html += "<p class='hint'>A kapcsolat megszakad, kérlek várj pár másodpercet, majd frissítsd az oldalt.</p>";
  html += "</div>";
  html += htmlFoot();
  server.send(200, "text/html", html);
  
  diagAdd("ESP32 kézi újraindítás webes felületről.");
  delay(1000); 
  ESP.restart();
}

void handleExpert() {
  if (!checkPinGuard()) return;
  String html = htmlHead("Expert Konfig", "8");

  html += "<div class='card wide'>"
          "<form action='/expertpost' method='POST'>"
          "<label>Modem hálózati mód (AT+CNMP)</label>"
          "<select name='cnmp'>"
          "<option value='2'>2G / GSM only</option>"
          "<option value='13'>GSM only (Alternatív)</option>"
          "<option value='38' selected>LTE-M / Cat-M (Auto)</option>"
          "<option value='51'>NB-IoT only</option>"
          "<option value='2'>Automatikus (Auto)</option>"
          "</select>"
          "<label>SMS útvonal (AT+CGSMS)</label>"
          "<select name='cgsms'>"
          "<option value='0'>0 - Csak PS (Csomagkapcsolt / LTE)</option>"
          "<option value='1' selected>1 - Csak CS (Áramkörkapcsolt / 2G)</option>"
          "<option value='2'>2 - PS preferred</option>"
          "<option value='3'>3 - CS preferred</option>"
          "</select>"
          "<label>LTE sávok engedélyezése (AT+CBANDCFG - Telekom: B3, B8, B20)</label>"
          "<div class='cb-row'><input type='checkbox' name='b3' id='b3Cb' checked><label for='b3Cb'>Band 3 (1800 MHz)</label></div>"
          "<div class='cb-row'><input type='checkbox' name='b8' id='b8Cb' checked><label for='b8Cb'>Band 8 (900 MHz)</label></div>"
          "<div class='cb-row'><input type='checkbox' name='b20' id='b20Cb' checked><label for='b20Cb'>Band 20 (800 MHz - Legfontosabb vidéken)</label></div>"
          "<label>IoT hálózati technológia (AT+CMNB)</label>"
          "<select name='cmnb'>"
          "<option value='1' selected>1 - Cat-M (LTE-M)</option>"
          "<option value='2'>2 - NB-IoT</option>"
          "<option value='3'>3 - Cat-M és NB-IoT kombinált</option>"
          "</select>"
          "<div style='display:flex;gap:10px;margin-top:20px'>"
          "<button type='submit' style='flex:2'>OK (Elküldés és mentés)</button>"
          "<button type='button' class='sec' onclick='location.href=\"/expertreset\"' style='flex:1'>Visszaállít (Alapértelmezett)</button>"
          "</div>"
          "</form>"
          "<hr style='border:0; border-top:1px solid var(--border); margin:20px 0;'>"
          "<h2>Teljes gyári reset</h2>"
          "<p class='hint'>Minden modem expert beállítás visszaállítása gyári alapértelmezettre (AT&F).</p>"
          "<form action='/expertfullreset' method='POST'>"
          "<button class='danger' style='margin-top:8px'>Teljes Reset (AT&F)</button>"
          "</form>"
          "</div>";
  html += "<form action='/esprestart' method='POST' style='margin-top:10px'>"
          "<button class='danger'>ESP32 Teljes Újraindítás</button></form>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleExpertPost() {
  if(sendModemBusyPage("Expert Mentes", "8", "/expert")) return;

  String cnmp = server.hasArg("cnmp") ? server.arg("cnmp") : "";
  String cgsms = server.hasArg("cgsms") ? server.arg("cgsms") : "";
  String cmnb = server.hasArg("cmnb") ? server.arg("cmnb") : "";
  
  String bands = "1";
  if(server.hasArg("b3")) bands += ",3";
  if(server.hasArg("b8")) bands += ",8";
  if(server.hasArg("b20")) bands += ",20";

  String resultLog = modemApplyExpertConfig(cnmp, cgsms, bands, cmnb);

  String html = htmlHead("Expert Mentes", "8");
  html += "<h1>Expert Konfiguráció Eredménye</h1>";
  html += "<div class='card wide'><div class='diag'>";
  html += resultLog;
  html += "</div><a href='/expert'><button class='sec' style='margin-top:14px'>Vissza az Expert oldalra</button></a></div>";
  html += htmlFoot();
  
  diagAdd("Expert AT konfiguráció elküldve.");
  server.send(200, "text/html", html);
}

void handleExpertReset() {
  if(sendModemBusyPage("Expert Reset", "8", "/expert")) return;
  
  modemResetExpertConfig();

  diagAdd("Expert beállítások visszaállítva gyári alapértelmezettre.");
  server.sendHeader("Location", "/expert");
  server.send(302);
}

void handleExpertFullReset() {
  if (sendModemBusyPage("Teljes Reset", "8", "/expert")) return;
  
  if (gModem.ready) {
    modemAtQuery("AT&F", 3000);
    modemAtQuery("AT&W", 3000);
  }
  
  diagAdd("Modem teljes gyári reset (AT&F) végrehajtva.");
  server.sendHeader("Location", "/expert");
  server.send(302);
}

void handleDiag() {
  String html = htmlHead("Diagnosztika", "5");

  html += "<script>"
          "function copyElement(id){"
            "var e=document.getElementById(id);"
            "if(!e)return;"
            "var text = e.innerText;"
            "navigator.clipboard.writeText(text).then(function() {"
              "alert('Vágólapra másolva!');"
            "}).catch(function(err) {"
              "console.error('Hiba a másolásnál: ', err);"
              "alert('Másolás sikertelen.');"
            "});"
          "}"
          "</script>";
        
  html += "<div class='card'><h2>Rendszer</h2>";
  html += stateRow("Free heap", String(ESP.getFreeHeap()/1024)+" KB");
  html += stateRow("Uptime", String(millis()/60000)+" perc");
  html += stateRow("NTP", gTime.synced ? gTime.localTime : (gTime.started ? "folyamatban" : "nem indult"), gTime.synced ? "g" : "y");
  html += stateRow("Init kiserletek", String(gModem.initAttempts));
  html += stateRow("AP SSID", gApSSID);
  html += stateRow("AP csatorna", String(gApChannel));
  html += stateRow("Kapcsolodott", String(WiFi.softAPgetStationNum())+" eszkoz");
  html += "</div>";

  if(gModem.ready){
    html += "<div class='card'><h2>Modem</h2>";
    html += stateRow("IMEI", "<span style='font-size:11px'>"+gModem.simIMEI+"</span>");
    html += stateRow("CCID", "<span style='font-size:11px'>"+gModem.simCCID+"</span>");
    html += stateRow("Operator", gModem.operatorName);
    html += stateRow("Jel (raw)", String(gModem.signalQuality));
    html += stateRow("Halozat tipus", gModem.netType);
    html += "</div>";
  }

  html += "<div class='card diag-card'><h2>Esemenyek <button class='sec' style='padding:4px 8px;font-size:11px;float:right;margin-top:-2px' onclick='copyElement(\"diagBox\")'>Másolás</button></h2>";
  html += "<div class='diag' id='diagBox'>";
  String log = diagDump();
  if(log.length()==0) log = "(meg nincs esemeny)";
  html += log;
  html += "</div></div>";

  html += "<div class='card wide'><h2>Beallitasok mentese & visszatoltese</h2>";
  html += "<a href='/eeprombackup'><button class='sec'>Beallitasok exportalasa</button></a>";
  html += "<form action='/eepromrestore' method='POST' style='margin-top:10px'>"
          "<label>Visszatoltendo adat</label>"
          "<textarea name='data' placeholder='Illeszd be ide az exportalt szoveget' "
          "style='min-height:60px;font-family:monospace;font-size:11px'></textarea>"
          "<button class='warn'>Visszatoltes</button>"
          "</form></div>";

  html += "<div class='card wide'><h2>AT parancs</h2>"
          "<label>Parancs</label>"
          "<div style='display:flex;gap:8px'>"
          "<input type='text' id='atCmdInput' placeholder='pl. AT+CSQ' autocomplete='off' autocapitalize='none' style='flex:1'>"
          "<button class='sec' type='button' onclick='sendAtCmd()' style='width:120px;margin-top:0'>Küldés</button>"
          "</div>"
          "<div id='atResultCard' style='display:none;margin-top:10px'>"
          "<label>Válasz <button class='sec' style='padding:2px 6px;font-size:10px;float:right;margin-top:-2px' onclick='copyElement(\"atResultBox\")'>Másolás</button></label>"
          "<div class='diag' id='atResultBox'></div>"
          "</div>"
          "<script>"
          "function sendAtCmd(){"
            "var input = document.getElementById('atCmdInput');var cmd = input.value;if(!cmd) return;"
            "var boxCard = document.getElementById('atResultCard');var box = document.getElementById('atResultBox');"
            "boxCard.style.display = 'block';box.innerText = 'Küldés folyamatban...';"
            "fetch('/at_ajax?cmd=' + encodeURIComponent(cmd))"
            ".then(function(r){ return r.text(); })"
            ".then(function(txt){ box.innerText = txt; input.value = ''; })"
            ".catch(function(){ box.innerText = 'Hiba történt.'; });"
          "}"
          "document.getElementById('atCmdInput').addEventListener('keydown', function(e){if(e.key==='Enter'){e.preventDefault();sendAtCmd();}});"
          "</script>";
  html += "<form action='/atstatus' method='POST' style='margin-top:10px'>"
          "<button class='warn'>AT allapot snapshot</button></form>";
  if(gAtStatusSnapshotAt > 0) html += "<div class='hint'>Legutobbi snapshot: " + ageText(gAtStatusSnapshotAt) + "</div>";
  html += "</div>";

  if(gAtStatusSnapshot.length() > 0) {
    html += "<div class='card diag-card'>"
            "<h2>Legutobbi AT allapot snapshot <button class='sec' style='padding:4px 8px;font-size:11px;float:right;margin-top:-2px' onclick='copyElement(\"atSnapshotBox\")'>Masolas</button></h2>"
            "<div class='diag' id='atSnapshotBox'>" + htmlEscape(gAtStatusSnapshot) + "</div></div>";
  }

  html += "<form action='/reinit' method='POST'>"
          "<button class='warn'>Modem ujraindit</button></form>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleAtAjax() {
  if(!server.hasArg("cmd")) {
    server.send(400, "text/plain", "Hianyzik a parancs");
    return;
  }
  String cmd = normalizeAtCommand(server.arg("cmd"));
  String resp = "";
  if(!gModem.ready) {
    resp = "HIBA: Modem nem aktiv.";
  } else {
    resp = modemAtQuery(cmd, 3000);
    diagAdd(cmd + " -> " + resp.substring(0, 40));
  }
  server.send(200, "text/plain", resp);
}

void handleAtStatus() {
  if(sendModemBusyPage("AT allapot", "5", "/diag")) return;
  diagAdd("AT allapot snapshot inditva");
  refreshAtStatusSnapshot();
  diagAdd("AT allapot snapshot kesz");
  server.sendHeader("Location","/diag");
  server.send(302);
}

void handleModemStatus() {
  String json = "{";
  json += "\"inProgress\":" + String(gModemInitRequested || gModem.initInProgress ? "true" : "false") + ",";
  json += "\"phase\":\"" + jsEscape(gModem.initPhase) + "\",";
  json += "\"phaseNum\":" + String(gModem.initPhaseNum) + ",";
  json += "\"phaseMax\":" + String(ModemState::INIT_PHASE_MAX) + ",";
  json += "\"ready\":" + String(gModem.ready ? "true" : "false") + ",";
  json += "\"uartResponding\":" + String(gModem.uartResponding ? "true" : "false") + ",";
  json += "\"lastError\":\"" + jsEscape(gModem.lastError) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleReinit() {
  if(sendModemBusyPage("Ujraindit", "1", "/")) return;
  diagAdd("Modem ujraindit (manualis)");
  gModem.ready=false; gModem.registered=false;
  gModemInitRequested = true;
  
  sendWaitPage("Modem Újraindítás", "A modem hardveres és szoftveres újraindítása folyamatban van. A hálózati regisztráció befejezéséig kérlek, várj.", "/", 35);
}