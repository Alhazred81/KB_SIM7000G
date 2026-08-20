//web_iot.cpp

#include "web_iot.h"
#include "web_common.h"
#include "config.h"       // ADDR_REPORT_TIMES miatt
#include "modem_mgr.h"    // gData, dataConnEnable() stb.
#include "NtfyClient.h"
#include "time_mgr.h"     // gTime miatt
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// --- Globális változók átemelve a web_ui-ból ---
extern WebServer server;
extern DataConnState gData;
extern TimeState gTime;
extern NtfyClient ntfy;
extern String gNtfyServer;
extern String gNtfyTopic;
extern String gNtfyNickname;
extern bool gNtfyStartupMsg;

extern void saveNtfyConfig(const String& server, const String& topic, const String& nickname, bool startupMsg);
extern String macSuffix();

// Ezek a változók mostantól itt élnek
String gReportTimes = "21:00";
int gLastSentMinute = -1;


void handleIot() {
  if (!checkPinGuard()) return;
  String html = htmlHead("IoT", "3");

  html += "<div class='card'><h2>Adatkapcsolat</h2>";
  html += stateRow("Állapot", gData.active ? "Aktív" : "Inaktív", gData.active ? "g" : "r");
  if (gData.active) html += stateRow("IP cím", gData.ip);
  
  html += "<div style='display:flex;gap:8px;margin-top:10px'>";
  if (gData.active) {
    html += "<form action='/dataon' method='POST' style='flex:1'><button class='sec' disabled>Adat be</button></form>";
    html += "<form action='/dataoff' method='POST' style='flex:1'><button class='danger'>Adat ki</button></form>";
  } else {
    html += "<form action='/dataon' method='POST' style='flex:1'><button>Adat be</button></form>";
    html += "<form action='/dataoff' method='POST' style='flex:1'><button class='sec' disabled>Adat ki</button></form>";
  }
  html += "</div></div>";

  html += "<div class='card'><h2>Ping Teszt</h2>";
  html += "<form action='/dataping' method='POST'>";
  html += "<input type='text' name='target' placeholder='IP vagy domain (pl. 8.8.8.8)'>";
  html += "<button class='sec'>Ping indítása</button></form>";
  if (gData.pingResult.length()) html += "<div class='msg " + String(gData.pingOk ? "ok" : "err") + "' style='margin-top:10px'>" + htmlEscape(gData.pingResult) + "</div>";
  html += "</div>";

  html += "<div class='card wide'><h2>📊 Napi Riport Időpontok (ntfy)</h2>"
          "<form action='/save-report' method='POST' onsubmit='return validateReportTimes()'>"
          "<label>Riport időpontok (HH:MM formátumban, ;-vel elválasztva)</label>"
          "<input type='text' name='report_times' id='reportTimesInput' value='" + (gReportTimes.length() ? gReportTimes : "21:00") + "' placeholder='pl. 08:00; 14:00; 21:00' required>"
          "<div id='timeError' style='color:var(--err); font-size:11px; margin-bottom:8px; display:none;'>Hibás formátum! Használd a HH:MM; HH:MM mintát (pl. 08:00; 21:00).</div>"
          "<p class='hint'>Az alapértelmezett beállítás este 21:00-kor küld jelentést. Több időpontot is megadhatsz pontosvesszővel elválasztva.</p>"
          "<button style='margin-top:4px'>Riport Konfig Mentése</button>"
          "</form>"
          "<form action='/test-report' method='POST' style='margin-top:10px'>"
          "<button class='sec'>🚀 Tesztriport küldése azonnal</button>"
          "</form></div>"
          
          "<script>"
          "function validateReportTimes() {"
          "  var val = document.getElementById('reportTimesInput').value.trim();"
          "  var parts = val.split(';');"
          "  var regex = /^([0-1]?[0-9]|2[0-3]):[0-5][0-9]$/;"
          "  for(var i=0; i<parts.length; i++) {"
          "    var t = parts[i].trim();"
          "    if(!regex.test(t)) {"
          "      document.getElementById('timeError').style.display = 'block';"
          "      return false;"
          "    }"
          "  }"
          "  document.getElementById('timeError').style.display = 'none';"
          "  return true;"
          "}"
          "</script>";

  html += "<div class='card wide'><h2>ntfy Értesítések</h2>";
  html += "<form action='/ntfy-send' method='POST'>";
  html += "<label>Üzenet küldése az aktuális csatornára</label>";
  html += "<input type='text' name='msg' placeholder='Írd be az értesítés szövegét...' required>";
  html += "<label>Prioritás</label>";
  html += "<select name='priority'>"
          "<option value='1'>1 - Min (Néma)</option>"
          "<option value='2'>2 - Low</option>"
          "<option value='3' selected>3 - Default</option>"
          "<option value='4'>4 - High</option>"
          "<option value='5'>5 - Max (Áttöri a némítást)</option>"
          "</select>";
  html += "<button style='margin-top:8px'>Küldés ntfy-ra</button>";
  html += "</form>";
  html += "<hr style='border:0; border-top:1px solid var(--border); margin:15px 0;'>";
  html += "<form action='/ntfy-poll' method='POST'>";
  html += "<button class='sec'>Üzenetek lekérdezése (Poll)</button>";
  html += "</form>";
  
  if (gData.lastError.length()) {
      if (gData.lastError.startsWith("NTFY_HTML:")) {
          html += "<div class='msg ok' style='margin-top:15px; text-align:left; line-height:1.4;'>" + gData.lastError.substring(10) + "</div>";
      } else if (gData.lastError.startsWith("NTFY")) {
          html += "<div class='msg ok' style='margin-top:10px'>" + htmlEscape(gData.lastError) + "</div>";
      } else {
          html += "<div class='msg err' style='margin-top:10px'>" + htmlEscape(gData.lastError) + "</div>";
      }
  }
  html += "</div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleNtfySend() {
  if(!server.hasArg("msg")) { 
    server.sendHeader("Location","/iot"); server.send(302); return; 
  }
  String msg = server.arg("msg");
  msg.trim();
  
  int prioVal = server.hasArg("priority") ? server.arg("priority").toInt() : 3;
  NtfyPriority priority = static_cast<NtfyPriority>(prioVal);

  String nick = gNtfyNickname;
  if (nick.length() == 0) nick = "szerver-" + macSuffix();

  bool ok = ntfy.send(msg.c_str(), nick.c_str(), priority);
  
  if(ok) {
    diagAdd("ntfy sikeresen elküldve: " + msg);
  } else {
    diagAdd("ntfy küldési hiba! HTTP code: " + String(ntfy.getLastHttpCode()));
  }
  
  server.sendHeader("Location","/iot");
  server.send(302);
}

void handleNtfyPoll() {
  NtfyPollResult res = ntfy.pollMessages("10m", ""); 
  
  if(res.success) {
    String raw = res.rawPayload;
    raw.trim();
    
    if (raw.length() == 0) {
        gData.lastError = "NTFY_HTML:<b style='font-size:14px;'>Beérkezett üzenetek:</b><br><br>Nincs új üzenet az elmúlt 10 percben.";
    } else {
        raw.replace("}{", "},{");
        raw.replace("}\r\n{", "},{");
        raw.replace("}\n{", "},{");
        raw.replace("}\r{", "},{");
        String jsonArray = "[" + raw + "]";
        
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, jsonArray);
        
        if (!err) {
            String formatted = "<b style='font-size:14px;'>Beérkezett üzenetek:</b><br><br>";
            JsonArray arr = doc.as<JsonArray>();
            
            if (arr.size() == 0) {
                formatted += "Nincs új üzenet az elmúlt 10 percben.";
            } else {
                for (JsonObject obj : arr) {
                    String title = obj["title"].as<String>();
                    if (title == "null" || title.length() == 0) title = "Értesítés";
                    String msg = obj["message"].as<String>();
                    if (msg == "null") msg = "";
                    int prio = obj["priority"] | 3;
                    
                    formatted += "<div style='margin-bottom:12px; padding-bottom:12px; border-bottom:1px solid var(--border);'>";
                    formatted += "<b style='color:var(--ok);'>" + htmlEscape(title) + "</b> <span style='font-size:10px; color:var(--txt2);'>(Prio: " + String(prio) + ")</span><br>";
                    formatted += "<span style='color:var(--txt);'>" + htmlEscape(msg) + "</span></div>";
                }
            }
            gData.lastError = "NTFY_HTML:" + formatted;
        } else {
            gData.lastError = "NTFY Válasz (nyers): " + res.rawPayload;
        }
    }
    diagAdd("ntfy poll sikeres.");
  } else {
    gData.lastError = "NTFY Poll hiba! Kód: " + String(res.httpCode);
    diagAdd("ntfy poll sikertelen.");
  }
  
  server.sendHeader("Location","/iot");
  server.send(302);
}

void handleSaveNtfy() {
  if (server.hasArg("ntfy_topic")) {
    String srv = server.hasArg("ntfy_server") ? server.arg("ntfy_server") : "ntfy.sh";
    String top = server.arg("ntfy_topic");
    String nick = server.hasArg("ntfy_nickname") ? server.arg("ntfy_nickname") : "";
    bool startup = server.hasArg("ntfy_startup");
    
    srv.trim();
    top.trim();
    nick.trim();
    
    saveNtfyConfig(srv, top, nick, startup);
    diagAdd("ntfy konfig mentve: " + srv + "/" + top + " (Indulási msg: " + String(startup ? "BE" : "KI") + ")");
  }
  server.sendHeader("Location", "/cfg");
  server.send(302);
}

void handleDataOn() {
  if(sendModemBusyPage("Adatkapcsolat", "3", "/iot")) return;
  String err = dataConnEnable();
  server.sendHeader("Location","/iot");
  server.send(302);
}

void handleDataOff() {
  if(sendModemBusyPage("Adatkapcsolat", "3", "/iot")) return;
  String err = dataConnDisable();
  server.sendHeader("Location","/iot");
  server.send(302);
}

void handleDataPing() {
  if(sendModemBusyPage("Ping", "3", "/iot")) return;
  String target = server.hasArg("target") ? server.arg("target") : "";
  target.trim();
  if(target.length() == 0) target = "8.8.8.8";
  dataConnPing(target);
  server.sendHeader("Location","/iot");
  server.send(302);
}

void saveReportConfig(const String& times) {
  for (int i = 0; i < 16; i++) {
    char c = (i < times.length()) ? times[i] : 0;
    EEPROM.write(ADDR_REPORT_TIMES + i, c);
  }
  EEPROM.commit();
}

String loadReportConfig() {
  String times = "";
  for (int i = 0; i < 16; i++) {
    char c = EEPROM.read(ADDR_REPORT_TIMES + i);
    if (c == 0) break;
    times += c;
  }
  return times.length() > 0 ? times : "21:00";
}

void handleSaveReport() {
  if (!server.hasArg("report_times")) {
    server.sendHeader("Location", "/iot");
    server.send(302);
    return;
  }
  
  String rTimes = server.arg("report_times");
  rTimes.trim();

  gReportTimes = rTimes.length() ? rTimes : "21:00";
  saveReportConfig(gReportTimes);

  diagAdd("Riport időpontok mentve EEPROM-ba: " + gReportTimes);
  server.sendHeader("Location", "/iot");
  server.send(302);
}

void handleTestReport() {
  if (sendModemBusyPage("Tesztriport", "3", "/iot")) return;

  String reportMsg = "🐝 **Kaptár Állapot Riport** \n\n";
  reportMsg += "| Azonosító | Család | Monitor | Beavatkozás |\n";
  reportMsg += "| :--- | :--- | :--- | :--- |\n";
  reportMsg += "| A1B2 | Rendben | OK | 🟡 5 nap |\n";
  reportMsg += "| C3D4 | Ellenőrzés | Gyenge jel | 🟠 2 nap |\n";
  reportMsg += "| E5F6 | Etetés | ⚠️ Akku (10%) | 🔴 Holnap |\n";
  reportMsg += "| G7H8 | Kezelés | ❌ Szenzor hiba | 🟣 Ma |\n";
  reportMsg += "| DEAD | 🔥 Hans | OFFLINE | 🔥 Hans |\n";

  String nick = gNtfyNickname;
  if (nick.length() == 0) nick = "szerver-" + macSuffix();

  int priorityVal = (reportMsg.indexOf("Hans") >= 0 || reportMsg.indexOf("🔥") >= 0) ? 5 : 2;

  bool ok = ntfy.send(reportMsg.c_str(), nick.c_str(), static_cast<NtfyPriority>(priorityVal));

  if (ok) {
    diagAdd("Tesztriport elküldve " + String(priorityVal) + "-ös prioritással.");
  } else {
    diagAdd("Tesztriport küldési hiba!");
  }

  server.sendHeader("Location", "/iot");
  server.send(302);
}

void checkAndSendScheduledReport() {
  if (!gTime.synced || gTime.localTime.length() < 5) return;

  String currentTimeStr = "";
  if (gTime.localTime.length() >= 5) {
    int colonIdx = gTime.localTime.indexOf(':');
    if (colonIdx >= 2) {
      currentTimeStr = gTime.localTime.substring(colonIdx - 2, colonIdx + 3);
    }
  }

  if (currentTimeStr.length() != 5) return;

  int currentTotalMins = currentTimeStr.substring(0, 2).toInt() * 60 + currentTimeStr.substring(3, 5).toInt();
  if (currentTotalMins == gLastSentMinute) return;

  String timesCopy = gReportTimes;
  while (timesCopy.length() > 0) {
    int semiIdx = timesCopy.indexOf(';');
    String singleTime = (semiIdx >= 0) ? timesCopy.substring(0, semiIdx) : timesCopy;
    singleTime.trim();
    
    if (singleTime.length() == 5 && singleTime == currentTimeStr) {
      diagAdd("Időzített napi riport indítása (" + singleTime + ")");
      
      String reportMsg = "🐝 **Kaptár Állapot Riport (Ütemezett)** \n\n";
      reportMsg += "| Azonosító | Család | Monitor | Beavatkozás |\n";
      reportMsg += "| :--- | :--- | :--- | :--- |\n";
      reportMsg += "| A1B2 | Rendben | OK (100%) | 🟡 5 nap |\n";
      reportMsg += "| C3D4 | Ellenőrzés | OK (50%) | 🟠 2 nap |\n";
      reportMsg += "| E5F6 | Etetés | ⚠️ Akku (10%) | 🔴 Holnap |\n";
      reportMsg += "| G7H8 | Kezelés | ❌ Szenzor hiba | 🟣 Ma |\n";
      reportMsg += "| DEAD | 🔥 Hans | OFFLINE | 🔥 Hans |\n";

      String nick = gNtfyNickname;
      if (nick.length() == 0) nick = "szerver-" + macSuffix();

      int priorityVal = (reportMsg.indexOf("Hans") >= 0 || reportMsg.indexOf("🔥") >= 0) ? 5 : 2;

      bool ok = ntfy.send(reportMsg.c_str(), nick.c_str(), static_cast<NtfyPriority>(priorityVal));
      if (ok) {
        diagAdd("Ütemezett riport sikeresen elküldve (" + singleTime + ").");
      } else {
        diagAdd("Ütemezett riport küldési hiba!");
      }

      gLastSentMinute = currentTotalMins;
      break;
    }

    if (semiIdx < 0) break;
    timesCopy = timesCopy.substring(semiIdx + 1);
  }
}   