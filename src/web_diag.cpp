// web_diag.cpp 

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h> 
#include "espnow.h"
#include "web_diag.h"
#include "web_common.h"
#include "modem_mgr.h"
#include "time_mgr.h"

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
          
  // --- ÚJ és JAVÍTOTT AT gombok ---
  html += "<div style='display:flex; gap:10px; margin-top:15px;'>";
  html += "<form action='/atstatus' method='POST' style='flex:1; margin:0;'>"
          "<button class='warn' style='width:100%; margin:0;'>AT allapot snapshot (Webre)</button></form>";
  html += "<button type='button' class='sec' style='flex:1; margin:0; border-color:#3b82f6; color:#3b82f6;' onclick='dumpModemSerial(this)'>📡 Frissít & Terminálba küld</button>";
  html += "</div>";

  html += "<script>"
          "function dumpModemSerial(btn) {"
          "  var origText = btn.innerText;"
          "  btn.innerText = '⏳ Készül...';"
          "  btn.disabled = true;"
          "  fetch('/api/atstatus_serial')"
          "    .then(function(r){"
          "      if(r.ok) alert('A legfrissebb AT Snapshot elkészült, nézd a Serial terminált!');"
          "      else alert('Hiba történt a kommunikáció során.');"
          "    }).catch(function(e){ alert('Hálózati hiba: ' + e); })"
          "    .finally(function(){"
          "      btn.innerText = origText;"
          "      btn.disabled = false;"
          "      location.reload();"
          "    });"
          "}"
          "</script>";

  if(gAtStatusSnapshotAt > 0) html += "<div class='hint' style='margin-top:5px;'>Legutobbi snapshot: " + ageText(gAtStatusSnapshotAt) + "</div>";
  html += "</div>";

  if(gAtStatusSnapshot.length() > 0) {
    html += "<div class='card diag-card'>"
            "<h2>Legutobbi AT allapot snapshot <button class='sec' style='padding:4px 8px;font-size:11px;float:right;margin-top:-2px' onclick='copyElement(\"atSnapshotBox\")'>Masolas</button></h2>"
            "<div class='diag' id='atSnapshotBox'>" + htmlEscape(gAtStatusSnapshot) + "</div></div>";
  }

  // --- KAPTÁR MENEDZSER (TESZTÜZEM) KÁRTYA ---
  html += "<div class='card'>";
  html += "<h2>🐝 Kaptár Menedzser (Adatbázis Tesztüzem)</h2>";
  html += "<p class='hint'>Adatbázis közvetlen kezelése és fiktív kaptárak létrehozása a műszerfal/térkép teszteléséhez.</p>";
  
  html += "<button class='sec' style='width:100%; margin-bottom:15px; border:1px dashed var(--accent); color:var(--accent);' onclick='addDummyHive()'>➕ Fiktív Teszt-Kaptár generálása</button>";
  html += "<div id='hive-list' style='margin-top:10px;'><i>Adatbázis betöltése...</i></div>";
  html += "</div>";

  html += "<script>"
          "function loadHives() {"
          "  fetch('/api/hives/list').then(r=>r.json()).then(data=>{"
          "    let h = '<table style=\"width:100%; text-align:left; font-size:13px; border-collapse:collapse;\">';"
          "    h += '<tr><th style=\"border-bottom:1px solid #444; padding:5px;\">ID</th><th style=\"border-bottom:1px solid #444; padding:5px;\">Anya</th><th style=\"border-bottom:1px solid #444; padding:5px;\">Művelet</th></tr>';"
          "    if(data.length === 0) { h += '<tr><td colspan=\"3\" style=\"text-align:center; padding:10px;\">Nincs regisztrált kaptár</td></tr>'; }"
          "    data.forEach(item => {"
          "      h += '<tr>';"
          "      h += '<td style=\"padding:5px;\"><b>' + item.id + '</b></td>';"
          "      h += '<td style=\"padding:5px;\">' + (item.queenVintage || '') + ' ' + (item.queenOrigin || '') + '</td>';"
          "      h += '<td style=\"padding:5px;\"><button style=\"background:#ef4444; padding:4px 8px; font-size:11px; margin:0;\" onclick=\"deleteHive(\\'' + item.id + '\\')\">Törlés</button></td>';"
          "      h += '</tr>';"
          "    });"
          "    h += '</table>';"
          "    document.getElementById('hive-list').innerHTML = h;"
          "  }).catch(e=>{ document.getElementById('hive-list').innerHTML = '<span style=\"color:red;\">Hiba a betöltéskor.</span>'; });"
          "}"
          "function addDummyHive() {"
          "  fetch('/api/hives/add_dummy', {method:'POST'}).then(r=>r.json()).then(d=>{"
          "    if(d.status==='ok') { loadHives(); }"
          "  });"
          "}"
          "function deleteHive(id) {"
          "  if(confirm('Biztosan véglegesen törlöd az adatbázisból: ' + id + '?')) {"
          "    fetch('/api/hives/delete?id=' + id, {method:'POST'}).then(r=>r.json()).then(d=>{"
          "      if(d.status==='ok') loadHives();"
          "    });"
          "  }"
          "}"
          "// Oldal betöltésekor automatikusan lekérjük a listát\n"
          "document.addEventListener('DOMContentLoaded', loadHives);"
          "</script>";

  // --- ESP-NOW TESZT ABLAK ---
  html += "<div class='card wide'>";
  html += "<h2>📡 ESP-NOW Forgalom Tesztelő</h2>";
  html += "<p class='hint'>Itt láthatod a beérkező csomagokat. A <b>[X s]</b> a szerver ideje (ebből látod a Deep Sleep hosszát), az <b>Ébrenlét</b> pedig a monitor boot idejét mutatja (ebből látszik az akkufogyasztás).</p>";
  
  html += "<div style='display:flex; gap:10px;'>";
  html += "<button class='sec' onclick='fetchEspNowLog()' style='flex:1;'>🔄 Frissítés</button>";
  html += "<button class='warn' onclick='fetch(\"/api/espnow_clear\").then(()=>fetchEspNowLog())' style='flex:1;'>🗑 Napló Törlése</button>";
  html += "</div>";
  
  html += "<div class='diag' id='espnowBox' style='min-height: 120px; margin-top: 10px; font-size:12px; white-space:pre-wrap;'>Várakozás az adatokra...</div>";
  html += "</div>";

  html += "<script>"
          "function fetchEspNowLog() {"
          "  fetch('/api/espnow_log').then(r=>r.text()).then(txt => {"
          "    document.getElementById('espnowBox').innerText = txt ? txt : '(Üres - még nem érkezett csomag)'; "
          "  });"
          "}"
          "setInterval(fetchEspNowLog, 3000);" // 3 másodpercenként automatikusan frissít
          "document.addEventListener('DOMContentLoaded', fetchEspNowLog);"
          "</script>";

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

void handleAtStatusSerial() {
  diagAdd("AT allapot snapshot inditva (Terminalbol kertek)");
  refreshAtStatusSnapshot();
  diagAdd("AT allapot snapshot kesz");
  
  Serial.println("\n========== 📡 MODEM AT-STÁTUSZ SNAPSHOT 📡 ==========");
  if (gAtStatusSnapshot.length() > 0) {
    Serial.println(gAtStatusSnapshot);
  } else {
    Serial.println("(Üres válasz, a modem valószínűleg nem válaszol.)");
  }
  Serial.println("=====================================================\n");
  
  server.send(200, "text/plain", "OK");
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

void handleGetHivesJson() {
  if (!checkPinGuard()) return;
  if (!LittleFS.exists("/hives.json")) {
    server.send(200, "application/json", "[]");
    return;
  }
  File file = LittleFS.open("/hives.json", "r");
  server.streamFile(file, "application/json");
  file.close();
}

void handleDeleteHive() {
  if (!checkPinGuard()) return;
  String id = server.arg("id");
  if (id == "") {
    server.send(400, "application/json", "{\"status\":\"error\", \"msg\":\"Missing ID\"}");
    return;
  }

  DynamicJsonDocument doc(4096);
  File file = LittleFS.open("/hives.json", "r");
  if (file) {
    deserializeJson(doc, file);
    file.close();
  } else {
    server.send(404, "application/json", "{\"status\":\"error\", \"msg\":\"No database\"}");
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  bool found = false;
  for (JsonArray::iterator it = arr.begin(); it != arr.end(); ++it) {
    if ((*it)["id"] == id) {
      arr.remove(it);
      found = true;
      break;
    }
  }

  if (found) {
    File outFile = LittleFS.open("/hives.json", "w");
    serializeJson(doc, outFile);
    outFile.close();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(404, "application/json", "{\"status\":\"not_found\"}");
  }
}

void handleAddDummyHive() {
  if (!checkPinGuard()) return;

  DynamicJsonDocument doc(4096);
  File file = LittleFS.open("/hives.json", "r");
  if (file) {
    deserializeJson(doc, file);
    file.close();
  } else {
    doc.to<JsonArray>();
  }
  
  JsonArray arr = doc.as<JsonArray>();
  JsonObject newHive = arr.createNestedObject();
  
  String dummyId = "TEST_" + String(random(1000, 9999));
  newHive["id"] = dummyId;
  newHive["queenOrigin"] = "Teszt Anya (Generált)";
  newHive["queenVintage"] = 2026;
  
  // Közeli koordináta a térkép teszteléséhez
  float latOffset = (random(-200, 200) / 100000.0);
  float lonOffset = (random(-200, 200) / 100000.0);
  newHive["lat"] = 47.514600 + latOffset;
  newHive["lon"] = 19.043500 + lonOffset;
  newHive["boxes"] = 3;
  
  JsonArray tags = newHive.createNestedArray("nfcTags");
  tags.add("DUMMY_TAG_1");

  File outFile = LittleFS.open("/hives.json", "w");
  serializeJson(doc, outFile);
  outFile.close();

  server.send(200, "application/json", "{\"status\":\"ok\", \"id\":\"" + dummyId + "\"}");
}

void handleApiEspNowLog() {
  if (!checkPinGuard()) return;
  server.send(200, "text/plain", getEspNowLog());
}

void handleApiEspNowClear() {
  if (!checkPinGuard()) return;
  clearEspNowLog();
  server.send(200, "text/plain", "OK");
}