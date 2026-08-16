#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "config.h"
#include "crypto.h"
#include "time_mgr.h"
#include "modem_mgr.h"
#include "gnss_mgr.h"
#include "wifi_sta.h"
#include "sensors.h"
#include "web_common.h"
#include "web_theme.h"

extern WebServer server;
extern DNSServer dnsServer;
extern String    gApSSID;
extern String    gApPass;
extern uint8_t   gApChannel;
extern unsigned long gLastSms;
extern bool      gDiagEnabled;
extern bool      gModemInitRequested;
extern bool      gSmsSendRequested;
extern String    gSmsPendingNum;
extern String    gSmsPendingText;
extern bool      gSmsSendInProgress;
extern bool      gSmsSendDone;
extern String    gSmsSendResult;

extern bool          gAtStatusInProgress;
extern String        gAtStatusSnapshot;
extern unsigned long gAtStatusSnapshotAt;


void handleCss() {
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.sendHeader("Content-Type",  "text/css");
  server.send_P(200, "text/css", CSS);
}

String stateRow(const String& key, const String& val, const String& cls="") {
  String s = "<div class='row'><span class='k'>";
  s += key;
  s += "</span><span class='v ";
  s += cls;
  s += "'>";
  s += val;
  s += "</span></div>";
  return s;
}

String sensorRowHtml(const String& sensorKey, const String& label, bool enabled,
                      bool hasEverRead, bool isOk, const String& valueText,
                      const String& pinInfo = "") {
  String color = "gray";
  if(enabled) color = (!hasEverRead) ? "y" : (isOk ? "g" : "r");
  String cssColor = (color=="g") ? "var(--ok)" : (color=="y") ? "var(--warn)" : (color=="r") ? "var(--err)" : "#555";

  String h = "<div class='sens-row' id='sensRow_" + sensorKey + "'>";
  h += "<label class='sens-toggle' style='--sens-color:" + cssColor + "'>"
       "<input type='checkbox' id='sensChk_" + sensorKey + "'"
       + String(enabled ? " checked" : "") +
       " onchange='sensToggle(\"" + sensorKey + "\", this.checked)'>"
       "<span class='slider'></span></label>";
  h += "<span class='sens-name'>" + label;
  if(pinInfo.length()) {
    h += "<span class='pin-info' onclick='this.classList.toggle(\"open\")'>&#9432;"
         "<span class='pin-bubble'>" + pinInfo + "</span></span>";
  }
  h += "</span>";
  h += "<span class='sens-value" + String(valueText.length() ? "" : " dim") + "' id='sensVal_" + sensorKey + "'>";
  h += valueText.length() ? valueText : (enabled ? "meres folyamatban..." : "kikapcsolva");
  h += "</span></div>";
  return h;
}
String modemBusyReason() {
  if(gModemInitRequested || gModem.initInProgress) return "Modem inicializalas folyamatban, varj amig befejezodik.";
  if(gSmsSendRequested || gSmsSendInProgress) return "SMS kuldes folyamatban, kozben a modem soros portja foglalt.";
  if(gData.inProgress) return "Adatkapcsolat valtas folyamatban, varj par masodpercet.";
  if(gData.pingInProgress) return "Ping teszt folyamatban, varj par masodpercet.";
  if(gModem.callActive) return "Hivas folyamatban, kozben a modem soros portjat nem piszkaljuk.";
  return "";
}

bool sendModemBusyPage(const String& title, const String& active, const String& backUrl) {
  String reason = modemBusyReason();
  if(reason.length() == 0) return false;
  String html = htmlHead(title, active);
  html += "<h1>Modem foglalt</h1>";
  html += "<div class='msg warn'>" + htmlEscape(reason) + "</div>";
  html += "<a href='" + backUrl + "'><button class='sec'>Vissza</button></a>";
  html += htmlFoot();
  server.send(200, "text/html", html);
  return true;
}

String modemAtQuery(const String& cmd, unsigned long timeoutMs = 1200) {
  modemDrain(25);
  modemSerial.println(cmd);
  String resp = modemReadUntilFinal(timeoutMs);
  resp.trim();
  if(resp.length() == 0) resp = "(ures valasz / timeout)";
  return resp;
}

void refreshAtStatusSnapshot() {
  static const char* cmds[] = {
    "AT", "ATI", "AT+CGMI", "AT+CGMM", "AT+CGMR", "AT+CGSN", "AT+CIMI", "AT+CCID",
    "AT+CPIN?", "AT+CSQ", "AT+COPS?", "AT+CREG?", "AT+CGREG?", "AT+CEREG?", "AT+CNSMOD?",
    "AT+CGATT?", "AT+CGACT?", "AT+CNACT?", "AT+CGDCONT?", "AT+CSCA?", "AT+CMGF?", "AT+CSCS?",
    "AT+CNMI?", "AT+CLCC", "AT+CCLK?", "AT+CBC", "AT+CGNSPWR?", "AT+CGNSINF", "AT+CGNSSINFO", "AT+CGNSANT"
  };
  static const uint16_t timeouts[] = {
    800, 1200, 1000, 1000, 1000, 1000, 1200, 1200,
    1200, 1000, 1800, 1000, 1000, 1000, 1200,
    1200, 1200, 1800, 1400, 1500, 1000, 1000,
    1000, 1000, 1200, 1200, 1200, 1800, 1800, 1200
  };
  const uint8_t count = sizeof(cmds) / sizeof(cmds[0]);

  gAtStatusInProgress = true;
  gAtStatusSnapshot = "========================================\n";
  gAtStatusSnapshot += "        AT ALLAPOT SNAPSHOT             \n";
  gAtStatusSnapshot += "========================================\n";
  gAtStatusSnapshot += "Ido: " + bestAvailableTimestamp() + "\n";
  gAtStatusSnapshot += "========================================\n\n";
  gAtStatusSnapshot.reserve(6000);

  for(uint8_t i = 0; i < count; i++) {
    String cmd = cmds[i];
    String resp = modemAtQuery(cmd, timeouts[i]);
    
    resp.replace("\r", "");
    while(resp.indexOf("\n\n") >= 0) {
      resp.replace("\n\n", "\n");
    }
    resp.trim();

    char numBuf[12];
    snprintf(numBuf, sizeof(numBuf), "[%02d/%02d] ", i + 1, count);

    gAtStatusSnapshot += String(numBuf) + cmd + "\n";
    gAtStatusSnapshot += "----------------------------------------\n";
    gAtStatusSnapshot += (resp.length() > 0 ? resp : "(ures valasz / timeout)") + "\n\n";
    
    Serial.println("[AT-STATUS] " + cmd + " -> " + resp.substring(0, 90));
    yield();
  }

  gAtStatusSnapshotAt = millis();
  gAtStatusInProgress = false;
}

void handleRoot() {
  String html = htmlHead("KB SIM7000G", "1");
  html += "<h1>KB SIM7000G</h1>";

  if(gModem.lastError.length()){
    html += smartErrorBox(gModem.lastError);
  }

  html += "<div class='card'><h2>Idő szinkronizáció</h2>";
  html += stateRow("NTP idő", gTime.synced ? gTime.localTime : "szinkron folyamatban", gTime.synced ? "g" : "y");
  String mTime = modemGetTime();
  html += stateRow("Modem idő (AT)", mTime.length() ? mTime : "nincs adat", mTime.length() ? "g" : "y");
  String gTimeStr = (gGnss.dateStr.length() && gGnss.timeStr.length()) ? (gGnss.dateStr + " " + gGnss.timeStr) : "";
  html += stateRow("GPS idő (GNSS)", gTimeStr.length() ? gTimeStr : "nincs fix", gGnss.fix ? "g" : "y");
  html += "</div>";

  html += "<div class='card'><h2>Modem & Halozat</h2>";
  html += stateRow("Modem", gModem.ready?"Aktiv":"Inaktiv", gModem.ready?"g":"r");
  if(gModem.initAttempts > 0) {
    html += stateRow("Modem UART", gModem.uartResponding?"Valaszol":"NEM valaszol", gModem.uartResponding?"g":"r");
  }
  html += stateRow("SIM", gModem.pinOk?"Feloldva":"Nincs feloldva", gModem.pinOk?"g":"y");
  html += stateRow("Halozat", gModem.registered?"Regisztralva":"Nem regisztralva", gModem.registered?"g":"r");
  if(gModem.registered){
    html += stateRow("Operator", gModem.operatorName.length()?gModem.operatorName:"–");
    html += stateRow("Jel", sigBar(gModem.signalQuality));
    html += stateRow("Tipus", gModem.netType.length()?gModem.netType:"–");
  }
  if(gModem.simCCID.length()) html += stateRow("SIM CCID", "<span style='font-size:11px'>"+gModem.simCCID+"</span>");
  if(gModem.simIMEI.length()) html += stateRow("IMEI", "<span style='font-size:11px'>"+gModem.simIMEI+"</span>");
  html += "</div>";

  html += "<div class='card'><h2>WiFi</h2>";
  if(gSta.mode == NetMode::STA_CONNECTED) {
    html += stateRow("Mod", "Kliens", "g");
    html += stateRow("SSID", htmlEscape(gSta.targetSSID));
    html += stateRow("IP", WiFi.localIP().toString());
    html += stateRow("Web", "http://" + WiFi.localIP().toString() + "/");
    html += stateRow("NTP ido", gTime.synced ? gTime.localTime : "szinkron folyamatban", gTime.synced ? "g" : "y");
  } else {
    html += stateRow("Mod", "AP", "y");
    html += stateRow("SSID", gApSSID);
    html += stateRow("IP", WiFi.softAPIP().toString());
    html += stateRow("Csatorna", String(gApChannel));
    html += stateRow("Kapcsolodott", String(WiFi.softAPgetStationNum())+" eszkoz");
  }
  html += "</div>";

  html += "<div class='card'><h2>GNSS</h2>";
  if(!gGnss.enabled) {
    html += stateRow("Allapot", "Kikapcsolva", "y");
  } else if(gGnss.fix) {
    char latS[16], lonS[16];
    dtostrf(gGnss.lat, 0, 5, latS);
    dtostrf(gGnss.lon, 0, 5, lonS);
    html += stateRow("Allapot", "FIX", "g");
    html += stateRow("Pozicio", String(latS)+", "+String(lonS));
    html += stateRow("Hasznalt muhold", String(gGnss.satUsed));
  } else {
    html += stateRow("Allapot", "Keresi a fixet...", "y");
    html += stateRow("GPS lathato", satText(gGnss.satGpsInView));
  }
  html += "<a href='/gnss'><button class='sec' style='margin-top:8px'>Reszletek &amp; muhold-bontas</button></a>";
  html += "</div>";

  html += "<form action='/reinit' method='POST'>";
  html += "<button class='sec'>&#128260; Modem ujraindit</button></form>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleComm() {
  String html = htmlHead("Kommunikacio", "2");
  html += "<h1>Kommunikacio</h1>";

  unsigned long left = 0;
  if(gLastSms > 0 && millis()-gLastSms < SMS_COOLDOWN_MS)
    left = (SMS_COOLDOWN_MS-(millis()-gLastSms))/1000;

  if(left > 0){
    html += "<div class='msg warn'>SMS kuldes varakozas: meg <span id='smsCd'>";
    html += String(left);
    html += "</span> mp</div>";
    html += "<script>"
            "(function(){"
              "var s=" + String(left) + ";"
              "var el=document.getElementById('smsCd');"
              "var t=setInterval(function(){"
                "s--;"
                "if(s<=0){clearInterval(t);"
                "location.reload();return;}"
                "el.innerText=s;"
              "},1000);"
            "})();"
            "</script>";
  }

  if(!gModem.ready){
    html += "<div class='msg err'>A modem nincs aktiv, SMS/hivas nem lehetseges.</div>";
  } else if(gModem.signalQuality==99 || gModem.signalQuality==0){
    html += "<div class='msg err'>Nincs GSM jel! Az SMS/hivas valoszinuleg nem fog sikerulni.</div>";
  } else if(gModem.signalQuality < 7){
    html += "<div class='msg warn'>Gyenge jel (" + String(gModem.signalQuality) +
            "/31) - az SMS/hivas sikertelen lehet.</div>";
  }

  String busy = modemBusyReason();
  if(gModem.ready && busy.length() == 0) {
    String smsc = getSmsc();
    html += "<div class='card wide'><h2>SMS-kozpont (SMSC)</h2>";
    if(smsc.length()) {
      html += stateRow("Aktualis szam", smsc, "g");
    } else {
      html += stateRow("Aktualis szam", "NINCS BEALLITVA", "r");
      html += "<div class='hint' style='margin-top:6px'>Ez tipikus oka a 'CMS ERROR 500' "
              "hibanak SMS kuldeskor. Telekom Domino eseten a hivatalos szam: "
              "<b>+36309888000</b>.</div>";
    }
    html += "<form action='/setsmsc' method='POST' style='margin-top:8px'>"
            "<label>SMSC szam beallitasa/frissitese</label>"
            "<input type='text' name='smsc' placeholder='+36309888000' "
            "value='" + htmlEscape(smsc) + "' maxlength='20'>"
            "<button class='sec'>Beallitas</button>"
            "</form></div>";
  } else if(gModem.ready) {
    html += "<div class='card wide'><h2>SMS-kozpont (SMSC)</h2>";
    html += stateRow("Allapot", "Most nem kerdezzuk le", "y");
    html += "<div class='hint'>" + htmlEscape(busy) + "</div></div>";
  }

  html += "<div class='card wide'><h2>Uj uzenet</h2>"
          "<form action='/dosms' method='POST' onsubmit='return prepNum_sms()'>";
  html += phoneInputBlock("smsBtn", "sms");
  html += "<label>Uzenet</label>"
          "<textarea name='smstext' id='st' maxlength='160' required "
          "oninput=\"document.getElementById('sc').innerText=this.value.length+'/160'\" "
          "placeholder='Csak ASCII karakterek (max 160)'></textarea>"
          "<div class='counter' id='sc'>0/160</div>";
  if(left > 0 || !gModem.ready)
    html += "<button disabled>Varakozas...</button>";
  else
    html += "<button type='submit' id='smsBtn' disabled>&#128228; Kuldés</button>";
  html += "</form></div>";

  html += "<div class='card wide'><h2>Hivas</h2>";
  if(gModem.callActive){
    html += "<div class='msg warn'>&#128222; Hivas folyamatban (csengetes: ";
    html += String(gModem.ringCount);
    html += "/3)</div>";
    html += "<form action='/hangup' method='POST'>"
            "<button class='danger'>&#128683; Bontás</button></form>";
  } else {
    html += "<form action='/docall' method='POST' onsubmit='return prepNum_call()'>";
    html += phoneInputBlock("callBtn", "call");
    if(!gModem.ready)
      html += "<button disabled>&#128222; Felhiv</button>";
    else
      html += "<button type='submit' id='callBtn' class='danger' disabled>&#128222; Felhiv</button>";
    html += "</form>";
  }
  html += "</div>";

  html += "<div class='card wide'><h2>Adatkapcsolat</h2>";
  html += stateRow("Allapot", gData.active ? "Aktiv" : "Kikapcsolva", gData.active ? "g" : "y");
  if(gData.active && gData.ip.length()) html += stateRow("IP cim", htmlEscape(gData.ip));
  html += stateRow("Halozat tipusa", gModem.netType.length() ? gModem.netType : "ismeretlen");
  if(gData.lastError.length()) html += "<div class='msg err' style='margin-top:6px'>" + htmlEscape(gData.lastError) + "</div>";

  html += "<div style='display:flex;gap:8px;margin-top:8px'>";
  html += "<form action='/dataon' method='POST' style='flex:1'><button class='sec'"
          + String(gData.active || !gModem.ready ? " disabled" : "") + ">Bekapcsolas</button></form>";
  html += "<form action='/dataoff' method='POST' style='flex:1'><button class='sec'"
          + String(!gData.active ? " disabled" : "") + ">Kikapcsolas</button></form>";
  html += "</div>";

  html += "<form action='/dataping' method='POST' style='margin-top:10px'>"
          "<label>Ping cel (IP cim)</label>"
          "<input type='text' name='target' value='1.1.1.1' placeholder='1.1.1.1' maxlength='45'>"
          "<button class='sec'" + String(!gData.active ? " disabled" : "") + ">Pingeles</button>"
          "</form>";
  if(gData.pingResult.length()) {
    html += "<div class='msg " + String(gData.pingOk ? "ok" : "err") + "' style='margin-top:8px'>"
            + htmlEscape(gData.pingResult) + "</div>";
  }
  html += "</div>";

  html += "<details class='card wide'><summary style='cursor:pointer;color:var(--accent);"
          "font-size:.92em;font-weight:700;text-transform:uppercase;letter-spacing:.5px'>"
          "Bejovo uzenetek (" + String(gSmsInboxCount) + ")</summary>";
  html += "<div style='margin-top:10px'>";
  html += stateRow("Osszesen fogadva (boot ota)", String(gSmsTotalReceived));
  {
    unsigned long elapsed = millis() - gLastSmsPoll;
    String nextCheck;
    if(gLastSmsPoll == 0 || elapsed >= SMS_POLL_INTERVAL_MS) {
      nextCheck = "hamarosan (a kovetkezo szabad pillanatban)";
    } else {
      nextCheck = String((SMS_POLL_INTERVAL_MS - elapsed) / 1000) + " mp mulva";
    }
    html += stateRow("Legkozelebbi ellenorzes", nextCheck);
  }
  if(gSmsInboxCount == 0) {
    html += "<div class='hint' style='margin-top:8px'>Meg nem erkezett uzenet (vagy a rendszer inditasa ota meg nem volt ellenorzes).</div>";
  } else {
    for(int i = 0; i < gSmsInboxCount; i++) {
      int idx = (gSmsInboxHead - 1 - i + gSmsInboxLimit) % gSmsInboxLimit;
      ReceivedSms& m = gSmsInbox[idx];
      html += "<div style='background:#0a0a18;border:1px solid var(--border);border-radius:10px;"
              "padding:10px 12px;margin-top:8px'>";
      html += "<div style='font-size:12px;color:var(--accent);font-weight:600'>" + htmlEscape(m.sender) + "</div>";
      html += "<div style='font-size:11px;color:var(--txt3);margin:2px 0'>"
              + (m.ourTimestamp.length() ? htmlEscape(m.ourTimestamp) : String("(nincs megbizhato idobelyeg)"))
              + " <span style='color:var(--txt3);opacity:.7'>| modem: " + htmlEscape(m.timestamp) + "</span></div>";
      html += "<div style='font-size:13px;color:var(--txt);margin-top:4px'>" + htmlEscape(m.text) + "</div>";
      html += "</div>";
    }
  }
  html += "</div></details>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleSetSmsc() {
  if(sendModemBusyPage("SMSC beallitas", "2", "/comm")) return;
  if(!server.hasArg("smsc")){
    server.sendHeader("Location","/comm"); server.send(302); return;
  }
  String smsc = server.arg("smsc");
  smsc.trim();

  String html = htmlHead("SMSC beallitas", "2");
  html += "<h1>SMSC beallitas</h1>";

  if(smsc.length() == 0) {
    html += "<div class='msg err'>Az SMSC szam nem lehet ures.</div>";
  } else {
    String err = setSmsc(smsc);
    if(err.length() == 0) {
      diagAdd("SMSC beallitva: " + smsc);
      html += "<div class='msg ok'>SMSC szam beallitva: " + htmlEscape(smsc) + "</div>";
    } else {
      diagAdd("SMSC beallitas HIBA: " + err);
      html += "<div class='msg err'>" + err + "</div>";
    }
  }
  html += "<a href='/comm'><button class='sec'>Vissza</button></a>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleDoSms() {
  if(sendModemBusyPage("SMS", "2", "/comm")) return;
  if(!server.hasArg("num") || !server.hasArg("smstext")){
    server.sendHeader("Location","/comm"); server.send(302); return;
  }

  unsigned long left = 0;
  if(gLastSms > 0 && millis()-gLastSms < SMS_COOLDOWN_MS)
    left = (SMS_COOLDOWN_MS-(millis()-gLastSms))/1000;
  if(left > 0){
    server.sendHeader("Location","/comm"); server.send(302); return;
  }

  String num     = server.arg("num");      num.trim();
  String smstext = server.arg("smstext");  smstext.trim();

  String clean = "";
  for(int i=0; i<(int)smstext.length() && i<SMS_MAX_LEN; i++){
    char c = smstext[i];
    if((uint8_t)c >= 0x80) continue;
    clean += c;
  }

  String html = htmlHead("SMS", "2");
  html += "<h1>SMS kuldés</h1>";

  if(!num.startsWith("+36")||num.length()!=12){
    html += "<div class='msg err'>Ervenytelen telefonszam! A formatum: +36xxxxxxxxx (9 szam a +36 utan).</div>";
    html += "<a href='/comm'><button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }
  if(clean.length()==0){
    html += "<div class='msg err'>Az uzenet ures maradt a ekezet-szures utan (csak ekezetes karaktereket irtal be?).</div>";
    html += "<a href='/comm'><button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  gSmsPendingNum  = num;
  gSmsPendingText = clean;
  gSmsSendDone    = false;
  gSmsSendResult  = "";
  gSmsSendRequested = true;

  html += "<div class='card full'>"
    "<div style='text-align:center;padding:8px'>"
    "<div id='smsPhase' style='font-size:13px;color:var(--txt2)'>SMS kuldese folyamatban...</div>"
    "<div id='smsSpin' style='font-size:28px;margin:10px 0'>&#9203;</div>"
    "<div id='smsResult' style='display:none'></div>"
    "<a href='/comm'><button id='smsWaitBtn' class='sec' disabled style='margin-top:12px'>Varakozas...</button></a>"
    "</div></div>"
    "<script>"
    "function smsPoll(){"
      "fetch('/smsstatus').then(function(r){return r.json();}).then(function(d){"
        "if(!d.done){"
          "setTimeout(smsPoll, 1000);"
          "return;"
        "}"
        "document.getElementById('smsSpin').style.display='none';"
        "document.getElementById('smsPhase').innerText = d.ok ? 'Kesz!' : 'Sikertelen.';"
        "var res = document.getElementById('smsResult');"
        "res.style.display='block';"
        "var btn = document.getElementById('smsWaitBtn');"
        "btn.disabled = false;"
        "btn.innerText = 'Vissza az SMS oldalra';"
        "btn.style.background = d.ok ? 'var(--ok)' : '';"
        "if(d.ok){"
          "res.innerHTML = \"<div class='msg ok'>&#10003; SMS elkuldve!</div>\";"
        "} else {"
          "res.innerHTML = \"<div class='msg err'>SMS kuldes sikertelen: \" + d.error + \"</div>\";"
        "}"
      "}).catch(function(){ setTimeout(smsPoll, 1500); });"
    "}"
    "setTimeout(smsPoll, 500);"
    "</script>";

  server.send(200, "text/html", html + htmlFoot());
}

void handleSmsStatus() {
  String json = "{";
  json += "\"done\":" + String(gSmsSendDone ? "true" : "false") + ",";
  json += "\"ok\":" + String(gSmsSendDone && gSmsSendResult.length()==0 ? "true" : "false") + ",";
  json += "\"error\":\"" + jsEscape(gSmsSendResult) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleDoCall() {
  if(sendModemBusyPage("Hivas", "2", "/comm")) return;
  if(!server.hasArg("num")){server.sendHeader("Location","/comm");server.send(302);return;}
  String num = server.arg("num"); num.trim();
  String html = htmlHead("Hivas", "2");
  html += "<h1>Hivasteszt</h1>";
  if(!num.startsWith("+36")||num.length()!=12){
    html += "<div class='msg err'>Ervenytelen szam! A formatum: +36xxxxxxxxx (9 szam a +36 utan).</div>";
  } else {
    String err = startCall(num);
    if(err.length()==0){
      diagAdd("Hivas inditva -> "+num);
      html += "<div class='msg ok'>&#128222; Hivas inditva: ";
      html += num;
      html += "</div>"
              "<div class='hint'>A hivas automatikusan bontodik: 3. csengetes, fogadas, visszautasitas vagy foglalt jel eseten.</div>"
              "<form action='/hangup' method='POST'>"
              "<button class='danger' style='margin-top:14px'>&#128683; Azonnali bontas</button></form>";
    } else {
      diagAdd("Hivas HIBA -> "+num+": "+err);
      html += "<div class='msg err'>Hivas inditas sikertelen: ";
      html += err;
      html += "</div>";
    }
  }
  html += "<a href='/comm'><button class='sec'>Vissza</button></a>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleHangup() {
  if(!gModem.callActive && sendModemBusyPage("Bontas", "2", "/comm")) return;
  hangUp();
  diagAdd("Hivas bontva (manualis)");
  server.sendHeader("Location","/comm");
  server.send(302);
}

void handleDataOn() {
  if(sendModemBusyPage("Adatkapcsolat", "2", "/comm")) return;
  String err = dataConnEnable();
  diagAdd(err.length() == 0 ? "Adatkapcsolat bekapcsolva, IP: " + gData.ip : "Adatkapcsolat HIBA: " + err);
  server.sendHeader("Location","/comm");
  server.send(302);
}

void handleDataOff() {
  if(sendModemBusyPage("Adatkapcsolat", "2", "/comm")) return;
  String err = dataConnDisable();
  diagAdd(err.length() == 0 ? "Adatkapcsolat kikapcsolva." : "Adatkapcsolat kikapcsolas HIBA: " + err);
  server.sendHeader("Location","/comm");
  server.send(302);
}

void handleDataPing() {
  if(sendModemBusyPage("Ping", "2", "/comm")) return;
  String target = server.hasArg("target") ? server.arg("target") : "";
  target.trim();
  if(target.length() == 0) target = "1.1.1.1";
  dataConnPing(target);
  diagAdd("Ping " + target + ": " + gData.pingResult);
  server.sendHeader("Location","/comm");
  server.send(302);
}

void handleExpert() {
  String html = htmlHead("Expert Konfig", "8");
  html += "<h1>Expert Modem Konfiguracio</h1>";

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
          
          "</form></div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleExpertPost() {
  if(sendModemBusyPage("Expert Mentes", "8", "/expert")) return;

  String html = htmlHead("Expert Mentes", "8");
  html += "<h1>Expert Konfiguráció Eredménye</h1>";
  html += "<div class='card wide'><div class='diag'>";

  modem.sendAT("+CFUN=0"); modem.waitResponse(2000L);

  if(server.hasArg("cnmp")) {
    String val = server.arg("cnmp");
    modem.sendAT("+CNMP=" + val);
    String r = modemReadUntilFinal(2000);
    html += "AT+CNMP=" + val + " -> " + r + "\n";
  }

  if(server.hasArg("cgsms")) {
    String val = server.arg("cgsms");
    modem.sendAT("+CGSMS=" + val);
    String r = modemReadUntilFinal(2000);
    html += "AT+CGSMS=" + val + " -> " + r + "\n";
  }

  String bands = "1";
  if(server.hasArg("b3")) bands += ",3";
  if(server.hasArg("b8")) bands += ",8";
  if(server.hasArg("b20")) bands += ",20";
  
  modem.sendAT("+CBANDCFG=\"CATM\"," + bands);
  String rBands = modemReadUntilFinal(2000);
  html += "AT+CBANDCFG=\"CATM\"," + bands + " -> " + rBands + "\n";

  if(server.hasArg("cmnb")) {
    String val = server.arg("cmnb");
    modem.sendAT("+CMNB=" + val);
    String r = modemReadUntilFinal(2000);
    html += "AT+CMNB=" + val + " -> " + r + "\n";
  }

  modem.sendAT("+CFUN=1"); modem.waitResponse(3000L);
  html += "\nModem rádió újraindítva (+CFUN=1). OK!\n";

  html += "</div>"
          "<a href='/expert'><button class='sec' style='margin-top:14px'>Vissza az Expert oldalra</button></a>"
          "</div>";

  html += htmlFoot();
  diagAdd("Expert AT konfiguráció elküldve.");
  server.send(200, "text/html", html);
}

void handleExpertReset() {
  if(sendModemBusyPage("Expert Reset", "8", "/expert")) return;
  
  modem.sendAT("+CFUN=0"); modem.waitResponse(2000L);
  modem.sendAT("+CNMP=38"); modem.waitResponse(1000L);
  modem.sendAT("+CGSMS=1"); modem.waitResponse(1000L);
  modem.sendAT("+CMNB=1"); modem.waitResponse(1000L);
  modem.sendAT("+CBANDCFG=\"CATM\",3,8,20"); modem.waitResponse(1000L);
  modem.sendAT("+CFUN=1"); modem.waitResponse(3000L);

  diagAdd("Expert beállítások visszaállítva gyári alapértelmezettre.");
  server.sendHeader("Location", "/expert");
  server.send(302);
}


String windSpeedValueText() {
  if(!gWindSpeed.enabled) return "";
  if(!gWindSpeed.lastReadOk && gWindSpeed.lastGoodRead == 0) return "";
  return String(gWindSpeed.speedMs, 1) + " m/s";
}

String windDirValueText() {
  if(!gWindDir.enabled) return "";
  if(!gWindDir.lastReadOk && gWindDir.lastGoodRead == 0) return "";
  return String(gWindDir.directionDeg, 0) + "\xC2\xB0 " + compassAbbrev(gWindDir.directionDeg);
}
String shtValueText() {
  if(!gSht.enabled) return "";
  if(!gSht.lastReadOk && gSht.lastGoodRead == 0) return "";
  return String(gSht.tempC, 1) + " C, " + String(gSht.humidityPct, 0) + "%";
}
String rainValueText() {
  if(!gRain.enabled) return "";
  return String(gRain.percentWet) + "% " + (gRain.isRaining ? "(esik)" : "(szaraz)");
}
String mpuValueText() {
  if(!gMpu.enabled) return "";
  if(!gMpu.lastReadOk && gMpu.lastGoodRead == 0) return "";
  return String(gMpu.accelX,2)+","+String(gMpu.accelY,2)+","+String(gMpu.accelZ,2)+" g";
}
String ahtBmpValueText() {
  if(!gAhtBmp.enabled) return "";
  if(!gAhtBmp.lastReadOk && gAhtBmp.lastGoodRead == 0) return "";
  String s = "";
  if(gAhtBmp.ahtOk) s += String(gAhtBmp.ahtTempC,1) + "C " + String(gAhtBmp.ahtHumidityPct,0) + "%";
  if(gAhtBmp.bmpOk) { if(s.length()) s += " | "; s += String(gAhtBmp.bmpPressureHpa,0) + "hPa"; }
  return s;
}
String ltrValueText() {
  if(!gLtr.enabled) return "";
  if(!gLtr.lastReadOk && gLtr.lastGoodRead == 0) return "";
  return "UVI " + String(gLtr.uvIndex, 1);
}

void handleSensors() {
  String html = htmlHead("Szenzorok", "7");
  html += "<h1>Szenzorok</h1>";



  html += "<div class='card wide'><h2>Allapot</h2>";
  html += sensorRowHtml("windspeed", "Szelsebesseg", gWindSpeed.enabled,
            gWindSpeed.lastGoodRead>0, gWindSpeed.lastReadOk, windSpeedValueText());
  html += sensorRowHtml("winddir", "Szelirany", gWindDir.enabled,
            gWindDir.lastGoodRead>0, gWindDir.lastReadOk, windDirValueText());
  html += sensorRowHtml("sht", "SHT57 ho/para", gSht.enabled,
            gSht.lastGoodRead>0, gSht.lastReadOk, shtValueText());
  html += sensorRowHtml("rain", "Esoszenzor", gRain.enabled,
            gRain.lastPoll>0, true, rainValueText());
  html += sensorRowHtml("mpu", "MPU6050 (I2C1)", gMpu.enabled,
            gMpu.lastGoodRead>0, gMpu.lastReadOk, mpuValueText());
  html += sensorRowHtml("ahtbmp", "AHT20+BMP280 (I2C2)", gAhtBmp.enabled,
            gAhtBmp.lastGoodRead>0, gAhtBmp.lastReadOk, ahtBmpValueText());
  html += sensorRowHtml("ltr", "LTR-390 UV (I2C2)", gLtr.enabled,
            gLtr.lastGoodRead>0, gLtr.lastReadOk, ltrValueText());
  html += "</div>";

  html += "<div class='card wide'><h2>RS485 / Modbus beallitasok</h2>"
          "<form action='/sensconfig' method='POST'>"
          "<label>RS485 baudrate</label>"
          "<select name='baud'>";
  const long bauds[] = {1200,2400,4800,9600,19200,38400,57600,115200};
  for(int i=0;i<8;i++){
    html += "<option value='" + String(bauds[i]) + "'";
    if((long)gSensRs485Baud == bauds[i]) html += " selected";
    html += ">" + String(bauds[i]) + "</option>";
  }
  html += "</select>"
          "<label>Szelsebesseg Modbus cim</label>"
          "<input type='number' name='addr_windspeed' min='1' max='247' value='" + String(gWindSpeed.modbusAddr) + "'>"
          "<label>Szelirany Modbus cim</label>"
          "<input type='number' name='addr_winddir' min='1' max='247' value='" + String(gWindDir.modbusAddr) + "'>"
          "<label>SHT57 Modbus cim</label>"
          "<input type='number' name='addr_sht' min='1' max='247' value='" + String(gSht.modbusAddr) + "'>"
          "<div class='cb-row'><input type='checkbox' name='rain_modbus' id='rmCb'"
          + String(gRain.isModbus ? " checked" : "") + "><label for='rmCb'>Esoszenzor RS485/Modbus modban (kulonben analog bemenet)</label></div>"
          "<label>Esoszenzor Modbus cim (csak ha fent bepipalva)</label>"
          "<input type='number' name='addr_rain' min='1' max='247' value='" + String(gRain.modbusAddr) + "'>"
          "<button class='sec'>Mentes</button>"
          "</form></div>";

  html += "<div class='card wide'><h2>Tesztelés</h2>"
          "<p class='hint'>Azonnali, egyszeri lekerdezes a kivalasztott eszkozre - "
          "akkor is mukodik, ha az adott szenzor meg nincs bekapcsolva (csak teszt celjabol).</p>"
          "<div style='display:flex;gap:8px;flex-wrap:wrap;margin-top:8px'>"
          "<form action='/senstest' method='POST' style='flex:1;min-width:120px'>"
          "<input type='hidden' name='which' value='windspeed'>"
          "<button class='sec'>Szelsebesseg teszt</button></form>"
          "<form action='/senstest' method='POST' style='flex:1;min-width:120px'>"
          "<input type='hidden' name='which' value='winddir'>"
          "<button class='sec'>Szelirany teszt</button></form>"
          "<form action='/senstest' method='POST' style='flex:1;min-width:120px'>"
          "<input type='hidden' name='which' value='sht'>"
          "<button class='sec'>SHT57 teszt</button></form>"
          "<form action='/senstest' method='POST' style='flex:1;min-width:120px'>"
          "<input type='hidden' name='which' value='rain'>"
          "<button class='sec'>Eso teszt</button></form>"
          "</div>";
  if(gLastSensTestResult.length()) {
    html += "<div class='msg " + String(gLastSensTestOk ? "ok" : "err") + "' style='margin-top:10px'>"
            + htmlEscape(gLastSensTestResult) + "</div>";
    if(gLastSensTestRaw.length()) {
      html += "<div class='diag' style='margin-top:6px'>Nyers Modbus valasz: " + htmlEscape(gLastSensTestRaw) + "</div>";
    }
  }
  html += "</div>";

  html += R"js(<script>
function sensToggle(key, on){
  fetch('/senstoggle', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'key='+encodeURIComponent(key)+'&on='+(on?'1':'0')});
}
function sensPoll(){
  fetch('/sensstatus').then(function(r){return r.json();}).then(function(d){
    for(var key in d){
      var row = document.getElementById('sensRow_'+key);
      if(!row) continue;
      var val = document.getElementById('sensVal_'+key);
      var chk = document.getElementById('sensChk_'+key);
      var s = d[key];
      if(val){
        val.innerText = s.value || (s.enabled ? 'meres folyamatban...' : 'kikapcsolva');
        val.className = 'sens-value' + (s.value ? '' : ' dim');
      }
      if(chk) chk.checked = s.enabled;
      var toggle = row.querySelector('.sens-toggle');
      if(toggle){
        var color = '#555';
        if(s.enabled) color = !s.hasEverRead ? 'var(--warn)' : (s.ok ? 'var(--ok)' : 'var(--err)');
        toggle.style.setProperty('--sens-color', color);
      }
    }
  }).catch(function(){});
}
setInterval(sensPoll, 3000);
</script>)js";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleSensToggle() {
  if(!server.hasArg("key") || !server.hasArg("on")) {
    server.send(400, "text/plain", "hianyzo parameter");
    return;
  }
  String key = server.arg("key");
  bool on = server.arg("on") == "1";

  int bit = -1;
  if(key == "windspeed") bit = SENS_BIT_WINDSPEED;
  else if(key == "winddir") bit = SENS_BIT_WINDDIR;
  else if(key == "sht") bit = SENS_BIT_SHT;
  else if(key == "rain") bit = SENS_BIT_RAIN;
  else if(key == "mpu") bit = SENS_BIT_MPU6050;
  else if(key == "ahtbmp") bit = SENS_BIT_AHT20BMP280;
  else if(key == "ltr") bit = SENS_BIT_LTR390;

  if(bit < 0) {
    server.send(400, "text/plain", "ismeretlen szenzor");
    return;
  }

  sensSetEnabled((uint8_t)bit, on);
  sensorsApplyEnabled();
  saveSensorConfig();
  diagAdd("Szenzor '" + key + "': " + (on ? "bekapcsolva" : "kikapcsolva"));
  server.send(200, "text/plain", "ok");
}

void handleSensStatus() {
  String json = "{";
  json += sensStatusJsonEntry("windspeed", gWindSpeed.enabled, gWindSpeed.lastGoodRead>0, gWindSpeed.lastReadOk, windSpeedValueText()) + ",";
  json += sensStatusJsonEntry("winddir", gWindDir.enabled, gWindDir.lastGoodRead>0, gWindDir.lastReadOk, windDirValueText()) + ",";
  json += sensStatusJsonEntry("sht", gSht.enabled, gSht.lastGoodRead>0, gSht.lastReadOk, shtValueText()) + ",";
  json += sensStatusJsonEntry("rain", gRain.enabled, gRain.lastPoll>0, true, rainValueText()) + ",";
  json += sensStatusJsonEntry("mpu", gMpu.enabled, gMpu.lastGoodRead>0, gMpu.lastReadOk, mpuValueText()) + ",";
  json += sensStatusJsonEntry("ahtbmp", gAhtBmp.enabled, gAhtBmp.lastGoodRead>0, gAhtBmp.lastReadOk, ahtBmpValueText()) + ",";
  json += sensStatusJsonEntry("ltr", gLtr.enabled, gLtr.lastGoodRead>0, gLtr.lastReadOk, ltrValueText());
  json += "}";
  server.send(200, "application/json", json);
}

void handleSensConfig() {
  if(server.hasArg("baud")) {
    long b = server.arg("baud").toInt();
    if(b >= 1200 && b <= 921600) {
      gSensRs485Baud = (uint32_t)b;
      if(gRs485Initialized) { rs485Init(); }
    }
  }
  if(server.hasArg("addr_windspeed")) {
    int v = server.arg("addr_windspeed").toInt();
    if(v >= 1 && v <= 247) gWindSpeed.modbusAddr = (uint8_t)v;
  }
  if(server.hasArg("addr_winddir")) {
    int v = server.arg("addr_winddir").toInt();
    if(v >= 1 && v <= 247) gWindDir.modbusAddr = (uint8_t)v;
  }
  if(server.hasArg("addr_sht")) {
    int v = server.arg("addr_sht").toInt();
    if(v >= 1 && v <= 247) gSht.modbusAddr = (uint8_t)v;
  }
  if(server.hasArg("addr_rain")) {
    int v = server.arg("addr_rain").toInt();
    if(v >= 1 && v <= 247) gRain.modbusAddr = (uint8_t)v;
  }
  gRain.isModbus = server.hasArg("rain_modbus");

  saveSensorConfig();
  diagAdd("Szenzor RS485/Modbus beallitasok mentve.");
  server.sendHeader("Location","/sensors");
  server.send(302);
}

void handleSensTest() {
  if(!server.hasArg("which")) {
    server.sendHeader("Location","/sensors"); server.send(302); return;
  }
  sensTestRun(server.arg("which"));
  diagAdd("Szenzor teszt (" + server.arg("which") + "): " + gLastSensTestResult);
  server.sendHeader("Location","/sensors");
  server.send(302);
}

void handleGnssStatus() {
  String json = "{";
  json += "\"enabled\":" + String(gGnss.enabled ? "true" : "false") + ",";
  json += "\"fix\":" + String(gGnss.fix ? "true" : "false") + ",";
  json += "\"lat\":" + String(gGnss.fix ? gGnss.lat : gGnss.assistLat, 6) + ",";
  json += "\"lon\":" + String(gGnss.fix ? gGnss.lon : gGnss.assistLon, 6) + ",";
  json += "\"alt\":" + String(gGnss.alt, 1) + ",";
  json += "\"speed\":" + String(gGnss.speed, 1) + ",";
  json += "\"course\":" + String(gGnss.course, 1) + ",";
  json += "\"hdop\":" + String(gGnss.hdop, 1) + ",";
  json += "\"satUsed\":" + String(gGnss.satUsed) + ",";
  json += "\"satView\":" + String(gGnss.satGpsInView);
  json += "}";
  server.send(200, "application/json", json);
}

void handleGnss() {
  String html = htmlHead("GPS", "6");
  html += "<h1>GPS / GNSS</h1>";

  if(!gModem.ready){
    html += "<div class='msg err'>A modem nincs aktiv, GNSS nem indithato.</div>";
  }

  html += "<div class='card'><h2>Vevo allapot</h2>";
  html += stateRow("Vevo", gnssReceiverStatusText(), gGnss.fix ? "g" : (gGnss.enabled ? "y" : "r"));
  html += stateRow("GNSS kapcsolo", gGnss.enabled ? "BE" : "KI", gGnss.enabled ? "g" : "r");
  html += stateRow("Run status", String(gGnss.runStatus));
  html += stateRow("Fix status", String(gGnss.fixStatus));
  html += stateRow("Utolso GNSS poll", ageText(gGnss.lastPoll));
  html += stateRow("Pozicio poll", ageText(gGnss.lastPositionPoll));
  html += stateRow("Muheld poll", ageText(gGnss.lastExtendedPoll));
  html += stateRow("Antenna poll", ageText(gGnss.lastAntennaPoll));
  html += stateRow("Inditas ota", gGnss.startedAt ? ageText(gGnss.startedAt) : "meg nem indult");
  html += stateRow("Utolsó fix", gGnss.lastGoodFix ? ageText(gGnss.lastGoodFix) : "meg nem volt");
  if(gGnss.lastError.length()) html += stateRow("Utolsó hiba", htmlEscape(gGnss.lastError), "y");
  html += stateRow("NTP ido", gTime.synced ? gTime.localTime : "nincs szinkron", gTime.synced ? "g" : "y");
  html += stateRow("GNSS UTC", gGnss.dateStr + " " + gGnss.timeStr);
  html += "</div>";

  html += "<div class='card wide'><h2>Kiindulo koordinata</h2>";
  html += stateRow("Latitude", String(gGnss.assistLat, 6));
  html += stateRow("Longitude", String(gGnss.assistLon, 6));
  html += "<p class='hint'>Alapertek: Budapest, Margit hid kozepe. Ez referencia-koordinata; a SIM7000 nem minden firmware-ben enged direkt hideginditasi pozicio-betaplalast AT paranccsal.</p>";
  html += "<form action='/gnssassist' method='POST'>";
  html += "<label>Latitude</label><input type='text' name='lat' value='" + String(gGnss.assistLat, 6) + "' inputmode='decimal'>";
  html += "<label>Longitude</label><input type='text' name='lon' value='" + String(gGnss.assistLon, 6) + "' inputmode='decimal'>";
  html += "<button class='sec'>Koordinata mentese</button></form>";
  html += "</div>";

  if(!gGnss.enabled){
    html += "<div class='card'><h2>GNSS kikapcsolva</h2>"
            "<p class='hint'>A helymeghatarozas jelenleg nincs bekapcsolva.</p>"
            "<form action='/gnssctl' method='POST'>"
            "<input type='hidden' name='action' value='start'>"
            "<button>&#128752; GNSS bekapcsolasa</button>"
            "</form></div>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  double activeLat = gGnss.fix ? gGnss.lat : gGnss.assistLat;
  double activeLon = gGnss.fix ? gGnss.lon : gGnss.assistLon;
  char latS[16], lonS[16];
  dtostrf(activeLat, 0, 6, latS);
  dtostrf(activeLon, 0, 6, lonS);

  html += "<div class='card'><h2>Pozicio";
  if(gGnss.fix) html += " <span style='color:var(--ok);font-size:11px'>&#9679; FIX</span>";
  else          html += " <span style='color:var(--warn);font-size:11px'>&#9679; Nincs fix (alapertelmezett koordinata)</span>";
  html += "</h2>";

  html += stateRow("Szelesseg", String(latS)+"&deg;");
  html += stateRow("Hosszusag", String(lonS)+"&deg;");
  html += stateRow("Magassag", String(gGnss.alt,1)+" m");
  html += stateRow("Sebesseg", String(gGnss.speed,1)+" km/h");
  html += stateRow("Irany", String(gGnss.course,1)+"&deg;");
  html += stateRow("HDOP", String(gGnss.hdop,1));

  html += "<div class='card wide' style='grid-column:1/-1'>"
          "<h2>Térkép</h2>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<div id='map' style='height:260px;border-radius:8px;margin-top:6px;z-index:1'></div>"
          "<script>"
          "var map = L.map('map').setView([" + String(latS) + ", " + String(lonS) + "], 15);"
          "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {maxZoom: 19, attribution: '&copy; OpenStreetMap'}).addTo(map);"
          "var marker = L.marker([" + String(latS) + ", " + String(lonS) + "]).addTo(map)"
            ".bindPopup('" + String(gGnss.fix ? "Aktuális fix" : "Kiinduló hely") + "').openPopup();"
          
          "var lastLat = " + String(latS) + ", lastLon = " + String(lonS) + ", lastFix = " + String(gGnss.fix ? "true" : "false") + ";"
          
          "function updateGnssMap(){"
            "fetch('/gnssstatus').then(function(r){return r.json();}).then(function(d){"
              "if(d.fix && (!lastFix || d.lat !== lastLat || d.lon !== lastLon)){"
                "marker.setLatLng([d.lat, d.lon]);"
                "map.setView([d.lat, d.lon], 16);"
                "marker.bindPopup('Aktuális fix').openPopup();"
                "lastLat = d.lat; lastLon = d.lon; lastFix = d.fix;"
              "}"
            "}).catch(function(){});"
          "}"
          "setInterval(updateGnssMap, 5000);"
          "</script>"
          "</div>";

  char mapUrl[96];
  snprintf(mapUrl, sizeof(mapUrl), "https://maps.google.com/?q=%s,%s", latS, lonS);
  html += "<a href='" + String(mapUrl) + "' target='_blank'><button class='sec' style='margin-top:10px'>&#128506; Megnyitas Google Maps-en</button></a>";
  html += "</div>";

  html += "<div class='card'><h2>M&#369;holdak</h2>";
  html += stateRow("Osszes hasznalt (fix eseten)", String(gGnss.satUsed));
  html += stateRow("GPS lathato (fix nelkul is)", satText(gGnss.satGpsInView));
  html += satRow("GPS", gGnss.satGPS);
  html += satRow("GLONASS", gGnss.satGLO);
  html += satRow("BeiDou", gGnss.satBDS);
  html += satRow("Galileo", gGnss.satGAL);
  html += "</div>";

  html += "<div class='card'><h2>GNSS antenna</h2>";
  bool hasSignal = (gGnss.satGpsInView > 0) || gGnss.fix;
  if(hasSignal) {
    html += stateRow("Levezetett allapot", "OK - jel erkezik (" + satText(gGnss.satGpsInView) + " GPS muhold lathato)", "g");
  } else if(gGnss.runStatus == 1) {
    html += stateRow("Levezetett allapot", "Meg nincs jel - keresi a muholdakat", "y");
  } else {
    html += stateRow("Levezetett allapot", "Nincs adat", "y");
  }
  html += "</div>";

  html += "<div class='card diag-card'><h2>Nyers GNSS valaszok</h2><div class='diag'>";
  html += "CGNSINF: " + htmlEscape(gGnss.rawCgnsinf) + "\n";
  html += "CGNSSINFO: " + htmlEscape(gGnss.rawCgnssinfo);
  html += "</div></div>";

  html += "<form action='/gnssctl' method='POST'>"
          "<input type='hidden' name='action' value='stop'>"
          "<button class='sec'>GNSS kikapcsolasa</button></form>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleGnssAssist() {
  if(!server.hasArg("lat") || !server.hasArg("lon")) {
    server.sendHeader("Location","/gnss"); server.send(302); return;
  }
  float lat = server.arg("lat").toFloat();
  float lon = server.arg("lon").toFloat();
  if(lat < -90 || lat > 90 || lon < -180 || lon > 180) {
    diagAdd("GNSS koordinata HIBA: ervenytelen tartomany");
  } else {
    gnssSaveAssist(lat, lon);
    diagAdd("GNSS kiindulo koordinata mentve: " + String(lat, 6) + ", " + String(lon, 6));
  }
  server.sendHeader("Location","/gnss");
  server.send(302);
}

void handleGnssCtl() {
  if(sendModemBusyPage("GNSS", "6", "/gnss")) return;
  String action = server.hasArg("action") ? server.arg("action") : "";
  if(action == "start") gnssStart();
  else if(action == "stop") gnssStop();
  server.sendHeader("Location","/gnss");
  server.send(302);
}

void handleCfg() {
  String html = htmlHead("Beallitasok", "4");
  html += "<h1>Beallitasok</h1>";

  html += "<div class='card wide'><h2>WiFi halozatra csatlakozas</h2>";

  if(gSta.mode == NetMode::STA_CONNECTED) {
    html += "<div class='msg ok'>Csatlakozva: <b>" + htmlEscape(gSta.targetSSID) + "</b><br>"
            "IP cim: " + gSta.ip + "</div>"
            "<form action='/stadisconnect' method='POST'>"
            "<button class='sec'>Kliens mod elhagyasa (vissza AP-ra)</button></form>";
  }
  else if(gSta.mode == NetMode::STA_CONNECTING) {
    html += "<div class='msg warn'>Csatlakozas folyamatban: " + htmlEscape(gSta.targetSSID) + "...</div>"
            "<div class='hint'>Ez akar 15 masodpercig is tarthat. Az oldal automatikusan frissul.</div>"
            "<meta http-equiv='refresh' content='3'>";
  }
  else {
    if(gSta.mode == NetMode::STA_FAILED && gSta.lastError.length()) {
      html += "<div class='msg err'>" + htmlEscape(gSta.lastError) + "</div>";
    }

    html += "<div id='netList'>";
    if(gScanCount == 0) {
      html += "<p class='hint'>Meg nincs lekerdezve halozatlista.</p>";
    } else {
      for(int i=0; i<gScanCount; i++) {
        int pct = constrain((gScanResults[i].rssi + 100) * 2, 0, 100);
        html += "<div class='netitem' onclick='pickNet(\"" + jsEscape(gScanResults[i].ssid) + "\"," +
                String(gScanResults[i].secure ? "true" : "false") + ")'>";
        html += "<span class='netname'>" + htmlEscape(gScanResults[i].ssid) + "</span>";
        html += "<span class='netmeta'>";
        if(gScanResults[i].secure) html += "&#128274; ";
        html += String(pct) + "%</span>";
        html += "</div>";
      }
    }
    html += "</div>";

    html += "<button type='button' class='sec' onclick='doScan()' id='scanBtn'>"
            "&#128260; Halozatok keresese</button>";

    html += "<div id='pwPopup' style='display:none;margin-top:10px'>"
            "<label id='pwLabel'>Jelszo</label>"
            "<input type='password' id='pwInput' placeholder='WiFi jelszo' autocomplete='off'>"
            "<button onclick='doConnect()' id='connectBtn'>Csatlakozas</button>"
            "<button type='button' class='sec' onclick='cancelPick()' style='margin-top:6px'>Megse</button>"
            "</div>";

    html += R"js(<style>
.netitem{display:flex;justify-content:space-between;align-items:center;
  padding:10px 12px;background:#0a0a18;border:1px solid var(--border);
  border-radius:10px;margin-bottom:6px;cursor:pointer;transition:.15s;gap:12px}
.netitem:active{background:#141428}
.netitem.picked{border-color:var(--accent)}
.netname{font-size:13px;color:var(--txt);overflow-wrap:anywhere}
.netmeta{font-size:11px;color:var(--txt2);white-space:nowrap}
</style>
<script>
var pickedSSID = null, pickedSecure = false;
function pickNet(ssid, secure){
  pickedSSID = ssid; pickedSecure = secure;
  document.getElementById('pwLabel').innerText = 'Jelszo (' + ssid + ')';
  document.getElementById('pwInput').value = '';
  document.getElementById('pwInput').style.display = secure ? 'block' : 'none';
  document.getElementById('pwPopup').style.display = 'block';
  document.getElementById('pwPopup').scrollIntoView({behavior:'smooth', block:'nearest'});
}
function cancelPick(){
  pickedSSID = null;
  document.getElementById('pwPopup').style.display = 'none';
}
function doConnect(){
  if(!pickedSSID) return;
  var pass = pickedSecure ? document.getElementById('pwInput').value : '';
  if(pickedSecure && pass.length < 8){
    alert('A jelszo legalabb 8 karakter (WPA2 minimum).');
    return;
  }
  var btn = document.getElementById('connectBtn');
  btn.disabled = true; btn.innerText = 'Csatlakozas...';
  var body = 'ssid=' + encodeURIComponent(pickedSSID) + '&pass=' + encodeURIComponent(pass);
  fetch('/staconnect', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:body})
    .then(function(r){ return r.text(); })
    .then(function(txt){
      if(txt === 'call-active'){
        alert('Aktiv hivas alatt nem lehet WiFi-t valtani. Probald ujra a hivas vege utan.');
        btn.disabled=false; btn.innerText='Csatlakozas';
        return;
      }
      location.reload();
    })
    .catch(function(){ btn.disabled=false; btn.innerText='Csatlakozas'; });
}
function doScan(){
  var btn = document.getElementById('scanBtn');
  btn.disabled = true; btn.innerText = 'Kereses...';
  fetch('/wifiscan', {method:'POST'})
    .then(function(r){ return r.text(); })
    .then(function(txt){
      if(txt === 'call-active'){
        alert('Aktiv hivas alatt nem lehet halozatot keresni. Probald ujra a hivas vege utan.');
        btn.disabled=false; btn.innerText='Halozatok keresese';
        return;
      }
      location.reload();
    })
    .catch(function(){ btn.disabled=false; btn.innerText='Halozatok keresese'; });
}
</script>)js";
  }
  html += "</div>";

  html += "<div class='card wide'><h2>WiFi AP</h2>"
          "<form action='/savewifi' method='POST'>"
          "<label>SSID vege (elotag: KB-teszt-)</label>"
          "<input type='text' name='ssid' value='";
  String macPart = gApSSID.length()>9 ? gApSSID.substring(9) : "";
  html += macPart;
  html += "' maxlength='20' placeholder='MAC-cim masodik fele'>"
          "<label>Jelszo (min. 8 kar.)</label>"
          "<input type='password' name='pass' value='' placeholder='ures = valtozatlan' maxlength='31'>"
          "<label>Csatorna</label>"
          "<select name='ch'>";
  for(int i=1;i<=13;i++){
    html += "<option value='";
    html += String(i);
    html += "'";
    if(i==gApChannel) html += " selected";
    html += ">Csatorna ";
    html += String(i);
    html += "</option>";
  }
  html += "</select><button>Mentes & ujraindulas</button></form></div>";

  html += "<div class='card'><h2>SIM PIN mentese</h2>"
          "<form action='/savepin' method='POST'>"
          "<label>PIN kod (4-8 szam)</label>"
          "<input type='password' name='pin' id='pi' maxlength='8' "
          "pattern='[0-9]{4,8}' placeholder='pl. 1234' oninput='pc()'>"
          "<button type='submit' id='pb' disabled>Mentes & csatlakozas</button>"
          "</form>"
          "<script>"
          "function pc(){var v=document.getElementById('pi').value;"
          "document.getElementById('pb').disabled=(v.length<4||!/^\\d+$/.test(v));}"
          "</script></div>";

  html += "<div class='card'><h2>SIM PIN csere</h2>"
          "<form action='/changepin' method='POST'>"
          "<label>Jelenlegi PIN</label>"
          "<input type='password' name='op' id='op' maxlength='8' oninput='cc()'>"
          "<label>Uj PIN</label>"
          "<input type='password' name='np1' id='np1' maxlength='8' oninput='cc()'>"
          "<label>Uj PIN megint</label>"
          "<input type='password' name='np2' id='np2' maxlength='8' oninput='cc()'>"
          "<div class='hint' id='ch'></div>"
          "<button type='submit' id='cb' disabled>PIN csere</button>"
          "</form>"
          "<script>"
          "function cc(){"
          "var o=document.getElementById('op').value,"
          "n1=document.getElementById('np1').value,"
          "n2=document.getElementById('np2').value,"
          "h=document.getElementById('ch'),"
          "b=document.getElementById('cb'),d=/^\\d+$/;"
          "b.disabled=true;h.style.color='var(--err)';"
          "if(o.length<4||!d.test(o)){h.innerText='Jelenlegi PIN: min. 4 szam.';return;}"
          "if(n1.length<4||!d.test(n1)){h.innerText='Uj PIN: min. 4 szam.';return;}"
          "if(o===n1){h.innerText='Az uj nem egyezhet a regivel!';return;}"
          "if(n1!==n2){h.innerText='A ket uj PIN nem egyezik!';return;}"
          "h.style.color='var(--ok)';h.innerText='Rendben.';b.disabled=false;}"
          "</script></div>";

  html += "<div class='card wide'><h2>LED / Panelverzio</h2>"
          "<form action='/savepanelver' method='POST'>"
          "<label>Panelverzio</label>"
          "<select name='ver' id='verSel' onchange='verChg()'>"
          "<option value='0'";
  if(gLed.mode == 0) html += " selected";
  html += ">V1.0 (GPIO12)</option>"
          "<option value='1'";
  if(gLed.mode == 1) html += " selected";
  html += ">V1.1 (GPIO13)</option>"
          "<option value='2'";
  if(gLed.mode == 2) html += " selected";
  html += ">Egyeni GPIO</option>"
          "<option value='3'";
  if(gLed.mode == 3) html += " selected";
  html += ">AT halozati LED (board sajat)</option>"
          "</select>";
  html += "<div id='customRow' style='display:";
  html += (gLed.mode == 2) ? "block" : "none";
  html += "'>"
          "<label>Egyeni GPIO szam</label>"
          "<input type='text' name='custompin' value='";
  html += String(gLed.customPin);
  html += "' maxlength='2' inputmode='numeric'>"
          "</div>"
          "<div class='hint'>AT modban a board sajat halozati LED-jet vezerli a modem (AT+CNETLIGHT), nincs sajat GPIO hasznalva.</div>"
          "<button>Mentes</button>"
          "</form>"
          "<script>"
          "function verChg(){"
          "var v=document.getElementById('verSel').value;"
          "document.getElementById('customRow').style.display=(v=='2')?'block':'none';"
          "}"
          "</script>"
          "<button id='ledTrigBtn' onclick='ledTrig()' class='";
  html += gLed.triggerOn ? "danger" : "sec";
  html += "' style='margin-top:6px'>";
  html += gLed.triggerOn ? "&#128161; LED KIKAPCSOLAS" : "&#128161; LED BEKAPCSOLAS (trigger)";
  html += "</button>"
          "<div class='hint' style='margin-top:6px' id='ledTrigHint'>Uzemmod: <b>";
  html += gLed.manualOverride
          ? ("MANUALIS - " + String(gLed.triggerOn ? "bekapcsolva" : "kikapcsolva"))
          : "Automatikus (halozat/GNSS villogas)";
  html += "</b></div>"
          "<button class='sec' onclick='ledAuto()' style='margin-top:6px'>Vissza automatikus villogasra</button>"
          "<script>"
          "function ledTrig(){"
            "var btn=document.getElementById('ledTrigBtn');"
            "btn.disabled=true;"
            "fetch('/ledtrigger',{method:'POST'})"
            ".then(function(r){return r.json();})"
            ".then(function(d){"
              "var hint=document.getElementById('ledTrigHint');"
              "if(d.on){"
                "btn.className='danger';"
                "btn.innerHTML='&#128161; LED KIKAPCSOLAS';"
                "hint.innerHTML='Uzemmod: <b>MANUALIS - bekapcsolva</b>';"
              "}else{"
                "btn.className='sec';"
                "btn.innerHTML='&#128161; LED BEKAPCSOLAS (trigger)';"
                "hint.innerHTML='Uzemmod: <b>MANUALIS - kikapcsolva</b>';"
              "}"
              "btn.disabled=false;"
            "})"
            ".catch(function(){btn.disabled=false;});"
          "}"
          "function ledAuto(){"
            "fetch('/ledauto',{method:'POST'}).then(function(){location.reload();});"
          "}"
          "</script>"
          "</div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleSavePanelVer() {
  if(!server.hasArg("ver")){ server.sendHeader("Location","/cfg"); server.send(302); return; }
  int ver = server.arg("ver").toInt();
  if(ver < 0 || ver > 3) ver = 0;

  int customPin = gLed.customPin;
  if(ver == 2 && server.hasArg("custompin")) {
    int cp = server.arg("custompin").toInt();
    if(cp >= 2 && cp <= 39) customPin = cp;
  }

  if(gLed.mode != 3) {
    int oldPin = currentLedGpio();
    if(oldPin >= 0) digitalWrite(oldPin, LOW);
  } else if(gLed.triggerOn) {
    setNetLightAT(false);
  }

  gLed.mode      = (uint8_t)ver;
  gLed.customPin = (uint8_t)customPin;
  gLed.triggerOn = false;       
  gLed.manualOverride = false;  
  saveLedConfig();
  ledPinReinit();

  diagAdd("Panelverzio/LED mod mentve: mode="+String(ver)+" pin="+String(customPin));
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleLedTrigger() {
  ledTrigger();
  diagAdd(String("LED trigger: ")+(gLed.triggerOn?"BE":"KI")+" (manualis)");
  String json = String("{\"on\":") + (gLed.triggerOn ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleLedAuto() {
  ledSetAuto();
  diagAdd("LED: vissza automatikus modba.");
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleWifiScan() {
  if(gModem.callActive) { server.send(200, "text/plain", "call-active"); return; }
  wifiScan();
  diagAdd("WiFi scan: " + String(gScanCount) + " halozat talalva");
  server.send(200, "text/plain", "ok");
}

void handleStaConnect() {
  if(gModem.callActive) { server.send(200, "text/plain", "call-active"); return; }
  if(!server.hasArg("ssid")) { server.send(400, "text/plain", "hianyzo ssid"); return; }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();
  diagAdd("WiFi STA csatlakozas inditva: " + ssid);
  server.send(200, "text/plain", "ok");
  wifiStaConnect(ssid, pass);
}

void handleStaDisconnect() {
  diagAdd("WiFi STA mod elhagyasa (manualis)");
  wifiStaDisconnect();
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleSaveWifi() {
  if(!server.hasArg("ssid")||!server.hasArg("pass")||!server.hasArg("ch")){
    server.sendHeader("Location","/cfg"); server.send(302); return;
  }
  String suffix = server.arg("ssid"); suffix.trim();
  String pass   = server.arg("pass"); pass.trim();
  int    ch     = server.arg("ch").toInt();

  if(suffix.length()>0) gApSSID = "KB-teszt-" + suffix;
  if(pass.length()>=8){ gApPass=pass; saveApPass(pass); }
  gApChannel = ch;
  EEPROM.write(ADDR_CHANNEL, ch); EEPROM.commit();

  diagAdd("WiFi mentve: "+gApSSID+" ch"+String(ch));
  server.sendHeader("Location","/cfg");
  server.send(302);
  delay(500);
  WiFi.softAPdisconnect(true); delay(200);
  WiFi.softAP(gApSSID.c_str(), gApPass.c_str(), gApChannel);
}

void handleSavePin() {
  if(!server.hasArg("pin")){server.sendHeader("Location","/cfg");server.send(302);return;}
  String pin = server.arg("pin"); pin.trim();
  savePin(pin);
  diagAdd("PIN mentve, modem ujraindul...");
  gModemInitRequested = true;
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleChangePin() {
  if(!server.hasArg("op")||!server.hasArg("np1")||!server.hasArg("np2")){
    server.sendHeader("Location","/cfg"); server.send(302); return;
  }
  String op=server.arg("op");op.trim();
  String n1=server.arg("np1");n1.trim();
  String err = changeSIMPin(op, n1);
  if(err.length()==0) diagAdd("SIM PIN megvaltoztatva.");
  else diagAdd("SIM PIN csere HIBA: "+err);
  server.sendHeader("Location","/cfg");
  server.send(302);
}

void handleEepromBackup() {
  uint8_t buf[EEPROM_SIZE];
  for(int i = 0; i < EEPROM_SIZE; i++) buf[i] = EEPROM.read(i);

  uint32_t checksum = 0;
  for(int i = 0; i < EEPROM_SIZE; i++) checksum = (checksum * 31) + buf[i];

  String b64 = base64Encode(buf, EEPROM_SIZE);

  String html = htmlHead("EEPROM export", "5");
  html += "<h1>Beallitasok exportja</h1>";
  html += "<div class='card full'>";
  html += "<p class='hint'>Masold ki es mentsd el ezt a szoveget egy biztonsagos helyre.</p>";
  html += "<textarea readonly style='min-height:140px;font-family:monospace;font-size:11px' "
          "onclick='this.select()'>" + String(EEPROM_SIZE) + ":" + String(checksum) + ":" + b64 + "</textarea>";
  html += "<a href='/diag'><button class='sec' style='margin-top:10px'>Vissza</button></a>";
  html += "</div>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleEepromRestore() {
  String html = htmlHead("EEPROM visszatoltes", "5");
  html += "<h1>Beallitasok visszatoltese</h1>";

  if(!server.hasArg("data") || server.arg("data").length() == 0) {
    html += "<div class='msg err'>Nincs beillesztett adat.</div>";
    html += "<a href='/diag'><button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  String data = server.arg("data");
  data.trim();

  int c1 = data.indexOf(':');
  int c2 = (c1 >= 0) ? data.indexOf(':', c1 + 1) : -1;
  if(c1 < 0 || c2 < 0) {
    html += "<div class='msg err'>Ervenytelen formatum - hianyzik a fejlec.</div>";
    html += "<a href='/diag'><button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  int declaredSize = data.substring(0, c1).toInt();
  uint32_t declaredChecksum = (uint32_t)data.substring(c1 + 1, c2).toInt();
  String b64 = data.substring(c2 + 1);

  if(declaredSize != EEPROM_SIZE) {
    html += "<div class='msg err'>Meret-eltero export (" + String(declaredSize) + " byte).</div>";
    html += "<a href='/diag'><button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  uint8_t buf[EEPROM_SIZE];
  size_t decoded = base64Decode(b64, buf, EEPROM_SIZE);

  if(decoded != (size_t)EEPROM_SIZE) {
    html += "<div class='msg err'>Hianyos/serult adat.</div>";
    html += "<a href='/diag'><button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  uint32_t checksum = 0;
  for(int i = 0; i < EEPROM_SIZE; i++) checksum = (checksum * 31) + buf[i];

  if(checksum != declaredChecksum) {
    html += "<div class='msg err'>Ellenorzo osszeg hiba - az adat serult.</div>";
    html += "<a href='/diag'><button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  for(int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, buf[i]);
  EEPROM.commit();

  diagAdd("EEPROM visszatoltve (manualis).");
  html += "<div class='msg ok'>Beallitasok sikeresen visszatoltve!</div>";
  html += "<form action='/reinit' method='POST'><button class='warn'>Modem ujraindit</button></form>";
  html += "<a href='/'><button class='sec' style='margin-top:8px'>Fooldal</button></a>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleDiag() {
  String html = htmlHead("Diagnosztika", "5");
  html += "<h1>Diagnosztika</h1>";

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

  html += "<div class='card diag-card'><h2>Esemenyek <button class='sec' style='padding:4px 8px;font-size:11px;float:right;margin-top:-2px' onclick='navigator.clipboard.writeText(document.getElementById(\"diagBox\").innerText);alert(\"Napló a vágólapra másolva!\")'>Másolás</button></h2>";
  html += "<div class='diag' id='diagBox'>";
  String log = diagDump();
  if(log.length()==0) log = "(meg nincs esemeny)";
  html += log;
  html += "</div></div>";

  html += "<div class='card wide'><h2>Beallitasok mentese &amp; visszatoltese</h2>";
  html += "<p class='hint'>A PIN, SIM-azonosito, WiFi jelszavak es egyeb beallitasok "
          "normal esetben tulelik a firmware ujrafeltoltest.</p>";
  html += "<a href='/eeprombackup'><button class='sec'>Beallitasok exportalasa</button></a>";
  html += "<form action='/eepromrestore' method='POST' style='margin-top:10px'>"
          "<label>Visszatoltendo adat (export-bol masolt szoveg)</label>"
          "<textarea name='data' placeholder='Illeszd be ide az exportalt szoveget' "
          "style='min-height:60px;font-family:monospace;font-size:11px'></textarea>"
          "<button class='warn'>Visszatoltes</button>"
          "</form></div>";

  html += "<div class='card wide'><h2>AT parancs</h2>"
          "<label>Parancs</label>"
          "<div style='display:flex;gap:8px'>"
          "<input type='text' id='atCmdInput' placeholder='pl. AT+CSQ vagy +CSQ' autocomplete='off' autocapitalize='none' style='flex:1'>"
          "<button class='sec' type='button' onclick='sendAtCmd()' style='width:120px;margin-top:0'>Küldés</button>"
          "</div>"
          "<div id='atResultCard' style='display:none;margin-top:10px'>"
          "<label>Válasz <button class='sec' style='padding:2px 6px;font-size:10px;float:right;margin-top:-2px' onclick='navigator.clipboard.writeText(document.getElementById(\"atResultBox\").innerText);alert(\"Válasz a vágólapra másolva!\")'>Másolás</button></label>"
          "<div class='diag' id='atResultBox'></div>"
          "</div>"
          "<script>"
          "function sendAtCmd(){"
            "var input = document.getElementById('atCmdInput');"
            "var cmd = input.value;"
            "if(!cmd) return;"
            "var boxCard = document.getElementById('atResultCard');"
            "var box = document.getElementById('atResultBox');"
            "boxCard.style.display = 'block';"
            "box.innerText = 'Küldés folyamatban...';"
            "fetch('/at_ajax?cmd=' + encodeURIComponent(cmd))"
            ".then(function(r){ return r.text(); })"
            ".then(function(txt){ box.innerText = txt; input.value = ''; })"
            ".catch(function(){ box.innerText = 'Hiba történt a lekérdezés során.'; });"
          "}"
          "document.getElementById('atCmdInput').addEventListener('keydown', function(e){"
            "if(e.key === 'Enter'){ e.preventDefault(); sendAtCmd(); }"
          "});"
          "</script>";
  html += "<form action='/atstatus' method='POST' style='margin-top:10px'>"
          "<button class='warn'>AT allapot snapshot</button></form>";
  if(gAtStatusSnapshotAt > 0) html += "<div class='hint'>Legutobbi snapshot: " + ageText(gAtStatusSnapshotAt) + "</div>";
  html += "</div>";

  if(gAtStatusSnapshot.length() > 0) {
    html += "<div class='card diag-card'><h2>Legutobbi AT allapot snapshot</h2><div class='diag'>";
    html += htmlEscape(gAtStatusSnapshot);
    html += "</div></div>";
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
  server.sendHeader("Location","/");
  server.send(302);
}

void handleNotFound() {
  if(server.uri() == "/s.css") { handleCss(); return; }
  server.sendHeader("Location","http://192.168.4.1/",true);
  server.send(302,"text/plain","");
}

void webBegin() {
  server.on("/s.css",        HTTP_GET,  handleCss);
  server.on("/",             HTTP_GET,  handleRoot);
  server.on("/comm",         HTTP_GET,  handleComm);
  server.on("/dataon",       HTTP_POST, handleDataOn);
  server.on("/dataoff",      HTTP_POST, handleDataOff);
  server.on("/dataping",     HTTP_POST, handleDataPing);
  server.on("/gnss",         HTTP_GET,  handleGnss);
  server.on("/gnssstatus",   HTTP_GET,  handleGnssStatus);
  server.on("/gnssctl",      HTTP_POST, handleGnssCtl);
  server.on("/gnssassist",   HTTP_POST, handleGnssAssist);
  server.on("/sensors",      HTTP_GET,  handleSensors);
  server.on("/sensconfig",   HTTP_POST, handleSensConfig);
  server.on("/senstoggle",   HTTP_POST, handleSensToggle);
  server.on("/sensstatus",   HTTP_GET,  handleSensStatus);
  server.on("/senstest",     HTTP_POST, handleSensTest);
  server.on("/expert",       HTTP_GET,  handleExpert);
  server.on("/expertpost",   HTTP_POST, handleExpertPost);
  server.on("/expertreset",  HTTP_POST, handleExpertReset);
  server.on("/cfg",          HTTP_GET,  handleCfg);
  server.on("/savewifi",     HTTP_POST, handleSaveWifi);
  server.on("/wifiscan",     HTTP_POST, handleWifiScan);
  server.on("/staconnect",   HTTP_POST, handleStaConnect);
  server.on("/stadisconnect",HTTP_POST, handleStaDisconnect);
  server.on("/savepin",      HTTP_POST, handleSavePin);
  server.on("/changepin",    HTTP_POST, handleChangePin);
  server.on("/savepanelver", HTTP_POST, handleSavePanelVer);
  server.on("/ledtrigger",   HTTP_POST, handleLedTrigger);
  server.on("/ledauto",      HTTP_POST, handleLedAuto);
  server.on("/diag",         HTTP_GET,  handleDiag);
  server.on("/at_ajax",      HTTP_GET,  handleAtAjax);
  server.on("/atstatus",     HTTP_POST, handleAtStatus);
  server.on("/eeprombackup",  HTTP_GET,  handleEepromBackup);
  server.on("/eepromrestore", HTTP_POST, handleEepromRestore);
  server.on("/setsmsc",      HTTP_POST, handleSetSmsc);
  server.on("/dosms",        HTTP_POST, handleDoSms);
  server.on("/smsstatus",    HTTP_GET,  handleSmsStatus);
  server.on("/docall",       HTTP_POST, handleDoCall);
  server.on("/hangup",       HTTP_POST, handleHangup);
  server.on("/modemstatus",  HTTP_GET,  handleModemStatus);
  server.on("/reinit",       HTTP_POST, handleReinit);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("[WEB] Szerver OK, port 80"));
}
String satRow(const String& systemName, int count) {
  return stateRow(systemName, satText(count));
}