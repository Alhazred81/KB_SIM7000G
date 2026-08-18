#include "web_ui.h"
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include "config.h"
#include "gnss_mgr.h"
#include "modem_mgr.h"
#include "NtfyClient.h"
#include "sensors.h"
#include "time_mgr.h"
#include "wifi_sta.h"
#include "web_common.h"
#include "web_theme.h"
#include "web_backup.h"

extern WebServer server;
extern DNSServer dnsServer;
extern ModemState gModem;
extern GnssState gGnss;
extern TimeState gTime;
extern WifiStaState gSta;
extern DataConnState gData;
extern LedConfig gLed;
extern NtfyClient ntfy;
extern String gNtfyServer;
extern String gNtfyTopic;
extern WindSpeedState gWindSpeed;
extern WindDirState gWindDir;
extern ShtSensorState gSht;
extern RainSensorState gRain;
extern Mpu6050State gMpu;
extern Aht20Bmp280State gAhtBmp;
extern Ltr390State gLtr;

extern String gApSSID;
extern String gApPass;
extern uint8_t gApChannel;
extern unsigned long gLastSms;
extern bool gDiagEnabled;
extern bool gModemInitRequested;

extern bool gSmsSendRequested;
extern String gSmsPendingNum;
extern String gSmsPendingText;
extern bool gSmsSendInProgress;
extern bool gSmsSendDone;
extern String gSmsSendResult;

extern ScannedNet gScanResults[];
extern int gScanCount;

extern void saveNtfyConfig(const String& server, const String& topic);

// PIN ellenőrző segédfüggvény a védett oldalakhoz
bool checkPinGuard() {
  if (loadPin().length() == 0) {
    server.sendHeader("Location", "/cfg");
    server.send(302);
    return false;
  }
  return true;
}

String stateRow(const String& key, const String& val, const String& cls = "") {
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
    h += "<span class='pin-info' onclick='this.classList.toggle(\"open\")'>ⓘ"
         "<span class='pin-bubble'>" + pinInfo + "</span></span>";
  }
  h += "</span>";
  h += "<span class='sens-value" + String(valueText.length() ? "" : " dim") + "' id='sensVal_" + sensorKey + "'>";
  h += valueText.length() ? valueText : (enabled ? "meres folyamatban..." : "kikapcsolva");
  h += "</span></div>";
  return h;
}

String satRow(const String& systemName, int count) {
  return stateRow(systemName, satText(count));
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

void sendWaitPage(const String& title, const String& message, const String& nextUrl, int waitSeconds) {
  String html = htmlHead(title, "");
  html += "<style>@keyframes spin { 100% { transform: rotate(360deg); } }</style>";
  html += "<div style='display:flex; justify-content:center; padding-top:40px;'>";
  html += "<div class='card' style='text-align:center; padding:40px 20px; max-width:400px; width:100%;'>";
  html += "<h2 style='font-size:18px; margin-bottom:15px;'>" + title + "</h2>";
  html += "<div style='font-size:40px; margin:20px 0; display:inline-block; animation:spin 3s linear infinite;'>⚙️</div>";
  html += "<p style='font-size:14px; color:var(--txt); margin-bottom:20px;'>" + message + "</p>";
  html += "<div class='msg warn' id='countdown' style='font-size:14px; font-weight:bold;'>Hátravan max: " + String(waitSeconds) + " mp</div>";
  html += "</div></div>";
  html += "<script>";
  
  html += "var w = " + String(waitSeconds) + ";";
  html += "var t = setInterval(function(){ "
          "  w--; "
          "  if(w > 0) document.getElementById('countdown').innerText = 'Hátravan max: ' + w + ' mp'; "
          "  else location.href='" + nextUrl + "'; "
          "}, 1000);";

  html += "var p = setInterval(function(){"
          "  fetch('/modemstatus').then(function(r){return r.json();}).then(function(d){"
          "    if(d && d.inProgress === false) { "
          "      clearInterval(t); clearInterval(p);"
          "      document.getElementById('countdown').innerText = 'Kész! Átirányítás...';"
          "      document.getElementById('countdown').className = 'msg ok';"
          "      setTimeout(function(){ location.href='" + nextUrl + "'; }, 500);"
          "    }"
          "  }).catch(function(){});"
          "}, 3000);";
          
  html += "</script>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

static String windSpeedValueText() {
  if(!gWindSpeed.enabled) return "";
  if(!gWindSpeed.lastReadOk && gWindSpeed.lastGoodRead == 0) return "";
  return String(gWindSpeed.speedMs, 1) + " m/s";
}

static String windDirValueText() {
  if(!gWindDir.enabled) return "";
  if(!gWindDir.lastReadOk && gWindDir.lastGoodRead == 0) return "";
  return String(gWindDir.directionDeg, 0) + "\xC2\xB0 " + compassAbbrev(gWindDir.directionDeg);
}

static String shtValueText() {
  if(!gSht.enabled) return "";
  if(!gSht.lastReadOk && gSht.lastGoodRead == 0) return "";
  return String(gSht.tempC, 1) + " C, " + String(gSht.humidityPct, 0) + "%";
}

static String rainValueText() {
  if(!gRain.enabled) return "";
  return String(gRain.percentWet) + "% " + (gRain.isRaining ? "(esik)" : "(szaraz)");
}

static String mpuValueText() {
  if(!gMpu.enabled) return "";
  if(!gMpu.lastReadOk && gMpu.lastGoodRead == 0) return "";
  return String(gMpu.accelX,2)+","+String(gMpu.accelY,2)+","+String(gMpu.accelZ,2)+" g";
}

static String ahtBmpValueText() {
  if(!gAhtBmp.enabled) return "";
  if(!gAhtBmp.lastReadOk && gAhtBmp.lastGoodRead == 0) return "";
  String s = "";
  if(gAhtBmp.ahtOk) s += String(gAhtBmp.ahtTempC,1) + "C " + String(gAhtBmp.ahtHumidityPct,0) + "%";
  if(gAhtBmp.bmpOk) { if(s.length()) s += " | "; s += String(gAhtBmp.bmpPressureHpa,0) + "hPa"; }
  return s;
}

static String ltrValueText() {
  if(!gLtr.enabled) return "";
  if(!gLtr.lastReadOk && gLtr.lastGoodRead == 0) return "";
  return "UVI " + String(gLtr.uvIndex, 1);
}

void handleRoot() {
  if (!LittleFS.exists("/index.html")) {
    server.send(404, "text/plain", "index.html nem talalhato a LittleFS-en");
    return;
  }
  File f = LittleFS.open("/index.html", "r");
  server.streamFile(f, "text/html");
  f.close();
}

void handleJs() {
  if (!LittleFS.exists("/app.js")) {
    server.send(404, "text/plain", "app.js nem talalhato a LittleFS-en");
    return;
  }
  File f = LittleFS.open("/app.js", "r");
  server.streamFile(f, "application/javascript");
  f.close();
}

void handleStyle() {
  if (!LittleFS.exists("/style.css")) {
    server.send(404, "text/plain", "style.css nem talalhato a LittleFS-en");
    return;
  }
  File f = LittleFS.open("/style.css", "r");
  server.streamFile(f, "text/css");
  f.close();
}

void handleHomeApi() {
  String json = "{";
  json += "\"pinSaved\":" + String(loadPin().length() > 0 ? "true" : "false") + ",";
  json += "\"modemReady\":" + String(gModem.ready ? "true" : "false") + ",";
  json += "\"registered\":" + String(gModem.registered ? "true" : "false") + ",";
  json += "\"operator\":\"" + jsEscape(gModem.operatorName) + "\",";
  json += "\"signal\":" + String(gModem.signalQuality) + ",";
  json += "\"netType\":\"" + gModem.netType + "\",";
  json += "\"gnssFix\":" + String(gGnss.fix ? "true" : "false") + ",";
  json += "\"lat\":" + String(gGnss.lat, 6) + ",";
  json += "\"lon\":" + String(gGnss.lon, 6) + ",";
  json += "\"sat\":" + String(gGnss.satUsed) + ",";
  json += "\"time\":\"" + gTime.localTime + "\"";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleNotFound() {
  if(server.uri() == "/s.css") { handleCss(); return; }
  server.sendHeader("Location","http://192.168.4.1/",true);
  server.send(302,"text/plain","");
}

void handleGsm() {
  if (!checkPinGuard()) return;
  String html = htmlHead("GSM", "2");
  html += "<h1>GSM / Hang / SMS</h1>";

  html += "<div class='card wide'><h2>SMS Küldés</h2>";
  html += "<form action='/dosms' method='POST'>";
  html += phoneInputBlock("smsBtn", "num");
  html += "<label>Üzenet</label><textarea name='smstext' maxlength='160'></textarea>";
  html += "<button id='smsBtn' disabled>SMS Küldés</button></form></div>";

  html += "<div class='card'><h2>Hívásteszt</h2>";
  html += "<form action='/docall' method='POST'>";
  html += phoneInputBlock("callBtn", "cnum");
  html += "<button id='callBtn' disabled>Hívás indítása</button></form></div>";

  html += "<div class='card'><h2>SMS Központ (SMSC)</h2>";
  html += "<form action='/setsmsc' method='POST'>";
  html += "<label>Központ száma (pl. +36309888000)</label>";
  html += "<input type='text' name='smsc' value='" + gModem.smscNumber + "'>";
  html += "<button class='sec'>Mentés</button></form></div>";

  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleIot() {
  if (!checkPinGuard()) return;
  String html = htmlHead("Internet / IoT", "3");
  html += "<h1>Internet / IoT</h1>";

  html += "<div class='card'><h2>Adatkapcsolat</h2>";
  html += stateRow("Állapot", gData.active ? "Aktív" : "Inaktív", gData.active ? "g" : "r");
  if (gData.active) html += stateRow("IP cím", gData.ip);
  if (gData.lastError.length()) html += "<div class='msg err'>" + htmlEscape(gData.lastError) + "</div>";
  html += "<div style='display:flex;gap:8px;margin-top:10px'>";
  html += "<form action='/dataon' method='POST' style='flex:1'><button>Adat be</button></form>";
  html += "<form action='/dataoff' method='POST' style='flex:1'><button class='sec'>Adat ki</button></form>";
  html += "</div></div>";

  html += "<div class='card'><h2>Ping Teszt</h2>";
  html += "<form action='/dataping' method='POST'>";
  html += "<input type='text' name='target' placeholder='IP vagy domain (pl. 8.8.8.8)'>";
  html += "<button class='sec'>Ping indítása</button></form>";
  if (gData.pingResult.length()) html += "<div class='msg " + String(gData.pingOk ? "ok" : "err") + "' style='margin-top:10px'>" + htmlEscape(gData.pingResult) + "</div>";
  html += "</div>";

  html += "<div class='card wide'><h2>ntfy Értesítések</h2>";
  html += "<form action='/ntfy-send' method='POST'>";
  html += "<label>Üzenet küldése az aktuális csatornára</label>";
  html += "<input type='text' name='msg' placeholder='Írd be az értesítés szövegét...' required>";
  html += "<button style='margin-top:8px'>Küldés ntfy-ra</button>";
  html += "</form>";
  html += "<hr style='border:0; border-top:1px solid var(--border); margin:15px 0;'>";
  html += "<form action='/ntfy-poll' method='POST'>";
  html += "<button class='sec'>Üzenetek lekérdezése (Poll)</button>";
  html += "</form>";
  if (gData.lastError.length() && gData.lastError.startsWith("NTFY")) {
      html += "<div class='msg ok' style='margin-top:10px'>" + htmlEscape(gData.lastError) + "</div>";
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
  
  bool ok = ntfy.send(msg.c_str(), "Kaptármonitor Értesítés");
  
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
    gData.lastError = "NTFY Válasz: " + res.rawPayload;
    diagAdd("ntfy poll sikeres. Adat: " + res.rawPayload);
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
    srv.trim();
    top.trim();
    saveNtfyConfig(srv, top);
    diagAdd("ntfy konfig mentve: " + srv + "/" + top);
  }
  server.sendHeader("Location", "/cfg");
  server.send(302);
}

void handleSetSmsc() {
  if(sendModemBusyPage("SMSC beallitas", "2", "/gsm")) return;
  if(!server.hasArg("smsc")){
    server.sendHeader("Location","/gsm"); server.send(302); return;
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
  html += "<a href='/gsm'><button class='sec'>Vissza</button></a>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleDoSms() {
  if(sendModemBusyPage("SMS", "2", "/gsm")) return;
  if(!server.hasArg("num") || !server.hasArg("smstext")){
    server.sendHeader("Location","/gsm"); server.send(302); return;
  }

  unsigned long left = 0;
  if(gLastSms > 0 && millis()-gLastSms < SMS_COOLDOWN_MS)
    left = (SMS_COOLDOWN_MS-(millis()-gLastSms))/1000;
  if(left > 0){
    server.sendHeader("Location","/gsm"); server.send(302); return;
  }

  String num    = server.arg("num");    num.trim();
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
    html += "<a href='/gsm'><button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }
  if(clean.length()==0){
    html += "<div class='msg err'>Az uzenet ures maradt a ekezet-szures utan (csak ekezetes karaktereket irtal be?).</div>";
    html += "<a href='/gsm'><button class='sec'>Vissza</button></a>";
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
    "<div id='smsSpin' style='font-size:28px;margin:10px 0'>⏳</div>"
    "<div id='smsResult' style='display:none'></div>"
    "<a href='/gsm'><button id='smsWaitBtn' class='sec' disabled style='margin-top:12px'>Varakozas...</button></a>"
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
          "res.innerHTML = \"<div class='msg ok'>✓ SMS elkuldve!</div>\";"
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
  if(sendModemBusyPage("Hivas", "2", "/gsm")) return;
  if(!server.hasArg("num")){server.sendHeader("Location","/gsm");server.send(302);return;}
  String num = server.arg("num"); num.trim();
  String html = htmlHead("Hivas", "2");
  html += "<h1>Hivasteszt</h1>";
  if(!num.startsWith("+36")||num.length()!=12){
    html += "<div class='msg err'>Ervenytelen szam! A formatum: +36xxxxxxxxx (9 szam a +36 utan).</div>";
  } else {
    String err = startCall(num);
    if(err.length()==0){
      diagAdd("Hivas inditva -> "+num);
      html += "<div class='msg ok'>📞 Hivas inditva: ";
      html += num;
      html += "</div>"
              "<div class='hint'>A hivas automatikusan bontodik: 3. csengetes, fogadas, visszautasitas vagy foglalt jel eseten.</div>"
              "<form action='/hangup' method='POST'>"
              "<button class='danger' style='margin-top:14px'>🚫 Azonnali bontas</button></form>";
    } else {
      diagAdd("Hivas HIBA -> "+num+": "+err);
      html += "<div class='msg err'>Hivas inditas sikertelen: ";
      html += err;
      html += "</div>";
    }
  }
  html += "<a href='/gsm'><button class='sec'>Vissza</button></a>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleHangup() {
  if(!gModem.callActive && sendModemBusyPage("Bontas", "2", "/gsm")) return;
  hangUp();
  diagAdd("Hivas bontva (manualis)");
  server.sendHeader("Location","/gsm");
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
  if(target.length() == 0) target = "1.1.1.1";
  dataConnPing(target);
  server.sendHeader("Location","/iot");
  server.send(302);
}

void handleSensors() {
  if (!checkPinGuard()) return;
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
          "<p class='hint'>Azonnali, egyszeri lekerdezes a kivalasztott eszkozre.</p>"
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
  if (!checkPinGuard()) return;
  // MIR Anti-Nuke Express stílusú cím és ikon
  String html = htmlHead("MIR Anti-Nuke Express", "6");
  html += "<h1>🛰️ MIR Anti-Nuke Express (GNSS)</h1>";

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
  html += "<form action='/gnssassist' method='POST'>";
  html += "<label>Latitude</label><input type='text' name='lat' value='" + String(gGnss.assistLat, 6) + "' inputmode='decimal'>";
  html += "<label>Longitude</label><input type='text' name='lon' value='" + String(gGnss.assistLon, 6) + "' inputmode='decimal'>";
  html += "<button class='sec'>Koordinata mentese</button></form>";
  html += "</div>";

  if(!gGnss.enabled){
    html += "<div class='card'><h2>GNSS kikapcsolva</h2>"
            "<form action='/gnssctl' method='POST'>"
            "<input type='hidden' name='action' value='start'>"
            "<button>🛰 MIR Műhold bekapcsolása</button>"
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
  if(gGnss.fix) html += " <span style='color:var(--ok);font-size:11px'>● CÉLBA VÉVE</span>";
  else          html += " <span style='color:var(--warn);font-size:11px'>● Célkeresztben</span>";
  html += "</h2>";

  html += stateRow("Szelesseg", String(latS)+"°");
  html += stateRow("Hosszusag", String(lonS)+"°");
  html += stateRow("Magassag", String(gGnss.alt,1)+" m");
  html += stateRow("Sebesseg", String(gGnss.speed,1)+" km/h");
  html += stateRow("Irany", String(gGnss.course,1)+"°");
  html += stateRow("HDOP", String(gGnss.hdop,1));

  html += "<div class='card wide' style='grid-column:1/-1'>"
          "<h2>Térkép / Célpont</h2>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<div id='map' style='height:260px;border-radius:8px;margin-top:6px;z-index:1'></div>"
          "<script>"
          "var map = L.map('map').setView([" + String(latS) + ", " + String(lonS) + "], 15);"
          "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {maxZoom: 19, attribution: '© OpenStreetMap'}).addTo(map);"
          "var marker = L.marker([" + String(latS) + ", " + String(lonS) + "]).addTo(map)"
            ".bindPopup('" + String(gGnss.fix ? "MIR Célpont" : "Bennszülött kiinduló hely") + "').openPopup();"
          "var lastLat = " + String(latS) + ", lastLon = " + String(lonS) + ", lastFix = " + String(gGnss.fix ? "true" : "false") + ";"
          "function updateGnssMap(){"
            "fetch('/gnssstatus').then(function(r){return r.json();}).then(function(d){"
              "if(d.fix && (!lastFix || d.lat !== lastLat || d.lon !== lastLon)){"
                "marker.setLatLng([d.lat, d.lon]);"
                "map.setView([d.lat, d.lon], 16);"
                "marker.bindPopup('MIR Célpont').openPopup();"
                "lastLat = d.lat; lastLon = d.lon; lastFix = d.fix;"
              "}"
            "}).catch(function(){});"
          "}"
          "setInterval(updateGnssMap, 5000);"
          "</script>"
          "</div>";

  char mapUrl[96];
  snprintf(mapUrl, sizeof(mapUrl), "https://maps.google.com/?q=%s,%s", latS, lonS);
  html += "<a href='" + String(mapUrl) + "' target='_blank'><button class='sec' style='margin-top:10px'>🗺 Megnyitas Google Maps-en</button></a>";
  html += "</div>";

  html += "<div class='card'><h2>Műholdak</h2>";
  html += stateRow("Osszes hasznalt", String(gGnss.satUsed));
  html += stateRow("GPS lathato", satText(gGnss.satGpsInView));
  html += satRow("GPS", gGnss.satGPS);
  html += satRow("GLONASS", gGnss.satGLO);
  html += satRow("BeiDou", gGnss.satBDS);
  html += satRow("Galileo", gGnss.satGAL);
  html += "</div>";

  html += "<div class='card diag-card'><h2>Nyers GNSS valaszok</h2><div class='diag'>";
  html += "CGNSINF: " + htmlEscape(gGnss.rawCgnsinf) + "\n";
  html += "CGNSSINFO: " + htmlEscape(gGnss.rawCgnssinfo);
  html += "</div></div>";

  html += "<form action='/gnssctl' method='POST'>"
          "<input type='hidden' name='action' value='stop'>"
          "<button class='sec'>MIR Műhold leállítása</button></form>";

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

void handleExpert() {
  if (!checkPinGuard()) return;
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
        if(gScanResults[i].secure) html += "🔒 ";
        html += String(pct) + "%</span>";
        html += "</div>";
      }
    }
    html += "</div>";

    html += "<button type='button' class='sec' onclick='doScan()' id='scanBtn'>"
            "🔄 Halozatok keresese</button>";

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
        alert('Aktiv hivas alatt nem lehet WiFi-t valtani.');
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
        alert('Aktiv hivas alatt nem lehet halozatot keresni.');
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
  html += "' maxlength='20'>"
          "<label>Jelszo (min. 8 kar.)</label>"
          "<input type='password' name='pass' value='' placeholder='ures = valtozatlan' maxlength='31'>"
          "<label>Csatorna</label>"
          "<select name='ch'>";
  for(int i=1;i<=13;i++){
    html += "<option value='" + String(i) + "'" + (i==gApChannel ? " selected" : "") + ">Csatorna " + String(i) + "</option>";
  }
  html += "</select><button>Mentes & ujraindulas</button></form></div>";

  html += "<div class='card wide'><h2>ntfy Beállítások (Üzenetcsatorna)</h2>"
          "<form action='/save-ntfy' method='POST'>"
          "<label>ntfy Szerver</label>"
          "<input type='text' name='ntfy_server' value='" + gNtfyServer + "'>"
          "<label>Topic neve (egyedi azonosító)</label>"
          "<input type='text' name='ntfy_topic' value='" + gNtfyTopic + "' required>"
          "<button style='margin-top:10px'>ntfy Mentés</button>"
          "</form></div>";

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
          "<label>Jelenlegi PIN</label><input type='password' name='op' id='op' maxlength='8' oninput='cc()'>"
          "<label>Uj PIN</label><input type='password' name='np1' id='np1' maxlength='8' oninput='cc()'>"
          "<label>Uj PIN megint</label><input type='password' name='np2' id='np2' maxlength='8' oninput='cc()'>"
          "<div class='hint' id='ch'></div>"
          "<button type='submit' id='cb' disabled>PIN csere</button></form>"
          "<script>"
          "function cc(){"
          "var o=document.getElementById('op').value,n1=document.getElementById('np1').value,n2=document.getElementById('np2').value,h=document.getElementById('ch'),b=document.getElementById('cb'),d=/^\\d+$/;"
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
          "<option value='0'" + String(gLed.mode == 0 ? " selected" : "") + ">V1.0 (GPIO12)</option>"
          "<option value='1'" + String(gLed.mode == 1 ? " selected" : "") + ">V1.1 (GPIO13)</option>"
          "<option value='2'" + String(gLed.mode == 2 ? " selected" : "") + ">Egyeni GPIO</option>"
          "<option value='3'" + String(gLed.mode == 3 ? " selected" : "") + ">AT halozati LED</option>"
          "</select>"
          "<div id='customRow' style='display:" + String(gLed.mode == 2 ? "block" : "none") + "'>"
          "<label>Egyeni GPIO szam</label>"
          "<input type='text' name='custompin' value='" + String(gLed.customPin) + "' maxlength='2' inputmode='numeric'></div>"
          "<button>Mentes</button></form>"
          "<script>"
          "function verChg(){document.getElementById('customRow').style.display=(document.getElementById('verSel').value=='2')?'block':'none';}"
          "</script>"
          "<button id='ledTrigBtn' onclick='ledTrig()' class='" + String(gLed.triggerOn ? "danger" : "sec") + "' style='margin-top:6px'>"
          + String(gLed.triggerOn ? "💡 LED KIKAPCSOLAS" : "💡 LED BEKAPCSOLAS (trigger)") + "</button>"
          "<div class='hint' style='margin-top:6px' id='ledTrigHint'>Uzemmod: <b>"
          + String(gLed.manualOverride ? (gLed.triggerOn ? "MANUALIS - bekapcsolva" : "MANUALIS - kikapcsolva") : "Automatikus") + "</b></div>"
          "<button class='sec' onclick='ledAuto()' style='margin-top:6px'>Vissza automatikus villogasra</button>"
          "<script>"
          "function ledTrig(){"
            "var btn=document.getElementById('ledTrigBtn');btn.disabled=true;"
            "fetch('/ledtrigger',{method:'POST'}).then(function(r){return r.json();}).then(function(d){"
              "var hint=document.getElementById('ledTrigHint');"
              "if(d.on){btn.className='danger';btn.innerHTML='💡 LED KIKAPCSOLAS';hint.innerHTML='Uzemmod: <b>MANUALIS - bekapcsolva</b>';}"
              "else{btn.className='sec';btn.innerHTML='💡 LED BEKAPCSOLAS (trigger)';hint.innerHTML='Uzemmod: <b>MANUALIS - kikapcsolva</b>';}"
              "btn.disabled=false;"
            "}).catch(function(){btn.disabled=false;});"
          "}"
          "function ledAuto(){fetch('/ledauto',{method:'POST'}).then(function(){location.reload();});}"
          "</script></div>";

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
  
  sendWaitPage("Modem Inicializálás", "A PIN kód mentve. A SIM7000G IoT modem hálózatkeresése szekvenciális, ami nagyjából fél percet vesz igénybe.", "/", 35);
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

void handleDiag() {
  String html = htmlHead("Diagnosztika", "5");
  html += "<h1>Diagnosztika</h1>";

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

void webBegin() {
  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/app.js",        HTTP_GET,  handleJs);
  server.on("/style.css",     HTTP_GET,  handleStyle);
  server.on("/s.css",         HTTP_GET,  handleCss);
  server.on("/api/home",      HTTP_GET,  handleHomeApi);
  server.on("/ntfy-send",     HTTP_POST, handleNtfySend);
  server.on("/ntfy-poll",     HTTP_POST, handleNtfyPoll);
  server.on("/save-ntfy",     HTTP_POST, handleSaveNtfy);
  server.on("/gsm",           HTTP_GET,  handleGsm);
  server.on("/iot",           HTTP_GET,  handleIot);
  
  server.on("/dataon",        HTTP_POST, handleDataOn);
  server.on("/dataoff",       HTTP_POST, handleDataOff);
  server.on("/dataping",      HTTP_POST, handleDataPing);
  
  server.on("/gnss",          HTTP_GET,  handleGnss);
  server.on("/gnssstatus",    HTTP_GET,  handleGnssStatus);
  server.on("/gnssctl",       HTTP_POST, handleGnssCtl);
  server.on("/gnssassist",    HTTP_POST, handleGnssAssist);
  
  server.on("/sensors",       HTTP_GET,  handleSensors);
  server.on("/sensconfig",    HTTP_POST, handleSensConfig);
  server.on("/senstoggle",    HTTP_POST, handleSensToggle);
  server.on("/sensstatus",    HTTP_GET,  handleSensStatus);
  server.on("/senstest",      HTTP_POST, handleSensTest);
  
  server.on("/expert",        HTTP_GET,  handleExpert);
  server.on("/expertpost",    HTTP_POST, handleExpertPost);
  server.on("/expertreset",   HTTP_POST, handleExpertReset);
  
  server.on("/cfg",           HTTP_GET,  handleCfg);
  server.on("/savewifi",      HTTP_POST, handleSaveWifi);
  server.on("/wifiscan",      HTTP_POST, handleWifiScan);
  server.on("/staconnect",    HTTP_POST, handleStaConnect);
  server.on("/stadisconnect", HTTP_POST, handleStaDisconnect);
  server.on("/savepin",       HTTP_POST, handleSavePin);
  server.on("/changepin",     HTTP_POST, handleChangePin);
  server.on("/savepanelver",  HTTP_POST, handleSavePanelVer);
  server.on("/ledtrigger",    HTTP_POST, handleLedTrigger);
  server.on("/ledauto",       HTTP_POST, handleLedAuto);
  
  server.on("/diag",          HTTP_GET,  handleDiag);
  server.on("/at_ajax",       HTTP_GET,  handleAtAjax);
  server.on("/atstatus",      HTTP_POST, handleAtStatus);
  server.on("/eeprombackup",  HTTP_GET,  handleEepromBackup);
  server.on("/eepromrestore", HTTP_POST, handleEepromRestore);
  server.on("/setsmsc",       HTTP_POST, handleSetSmsc);
  server.on("/dosms",         HTTP_POST, handleDoSms);
  server.on("/smsstatus",     HTTP_GET,  handleSmsStatus);
  server.on("/docall",        HTTP_POST, handleDoCall);
  server.on("/hangup",        HTTP_POST, handleHangup);
  server.on("/modemstatus",   HTTP_GET,  handleModemStatus);
  server.on("/reinit",        HTTP_POST, handleReinit);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("[WEB] Webszerver elindult."));
}