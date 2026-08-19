//web_gsm.cpp  

#include "web_gsm.h"
#include "modem_mgr.h"
#include "web_common.h"
#include <WebServer.h>

extern WebServer server;
extern ModemState gModem;
extern unsigned long gLastSms;
extern bool gSmsSendRequested;
extern String gSmsPendingNum;
extern String gSmsPendingText;
extern bool gSmsSendInProgress;
extern bool gSmsSendDone;
extern String gSmsSendResult;


void handleGsm() {
  if (!checkPinGuard()) return;
  String html = htmlHead("GSM", "2");

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

  html += "<div class='card wide'><h2>Hálózatválasztás (Automata / Kézi)</h2>";
  html += "<p class='hint'>Alapértelmezésben automata, de fix telepítésnél rögzítheted a saját szolgáltatód, hogy ne keresgéljen feleslegesen.</p>";
  
  html += stateRow("Jelenlegi operátor", gModem.operatorName.length() ? gModem.operatorName : "Ismeretlen");
  html += stateRow("Hálózati típus", gModem.netType.length() ? gModem.netType : "Ismeretlen");

  html += "<div style='display:flex;gap:8px;margin-top:12px;flex-wrap:wrap'>";
  html += "<form action='/netauto' method='POST' style='flex:1;min-width:140px'><button class='sec'>🔄 Váltás Automatikusra</button></form>";
  html += "<form action='/netscan' method='POST' style='flex:1;min-width:140px'><button class='sec'>📡 Hálózatok keresése</button></form>";
  html += "</div>";

  html += "<form action='/netmanual' method='POST' style='margin-top:14px;border-top:1px solid var(--border);padding-top:12px'>";
  html += "<label>Kézi hálózat rögzítése (MCC/MNC kód, pl. Telekom: 21630)</label>";
  html += "<div style='display:flex;gap:8px'>";
  html += "<input type='text' name='netcode' placeholder='pl. 21630' style='flex:1;margin:0'>";
  html += "<button style='width:auto;padding:0 16px'>Rögzítés</button>";
  html += "</div></form>";

  html += "</div>";

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

void handleSetSmsc() {
  if(sendModemBusyPage("SMSC beallitas", "2", "/gsm")) return;
  if(!server.hasArg("smsc")){
    server.sendHeader("Location","/gsm"); server.send(302); return;
  }
  String smsc = server.arg("smsc");
  smsc.trim();

  String html = htmlHead("SMSC beallitas", "2");

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

void handleNetAuto() {
  if(sendModemBusyPage("Hálózatváltás", "2", "/gsm")) return;
  String err = setAutoNetwork();
  if(err.length() == 0) {
    diagAdd("Hálózat visszaállítva automatikus módra.");
  } else {
    diagAdd("Hiba automatikus hálózatváltáskor: " + err);
  }
  server.sendHeader("Location", "/gsm");
  server.send(302);
}

void handleNetScan() {
  if(sendModemBusyPage("Hálózatkeresés", "2", "/gsm")) return;
  diagAdd("Hálózatok keresése indítva (AT+COPS=)...");
  String rawRes = scanAvailableNetworks();
  diagAdd("Hálózat keresés eredménye: " + rawRes);
  
  String html = htmlHead("Hálózatválasztás", "2");
  html += "<h1>Elérhető mobilhálózatok</h1>";
  html += "<div class='card wide'>";
  html += "<p class='hint'>Válaszd ki az alábbi listából a kívánt hálózatot a rögzítéshez:</p>";

  html += "<form action='/netmanual' method='POST'>";
  html += "<label>Talált hálózatok</label>";
  html += "<select name='netcode' style='margin-bottom:12px'>";

  int pos = 0;
  bool foundAny = false;

  while(true) {
    int start = rawRes.indexOf('(', pos);
    if(start < 0) break;
    int end = rawRes.indexOf(')', start);
    if(end < 0) break;
    
    String entry = rawRes.substring(start + 1, end);
    pos = end + 1;

    String parts[10];
    int partCount = 0;
    int pIdx = 0;
    while(partCount < 10) {
      int q1 = entry.indexOf('"', pIdx);
      if(q1 < 0) break;
      int q2 = entry.indexOf('"', q1 + 1);
      if(q2 < 0) break;
      parts[partCount++] = entry.substring(q1 + 1, q2);
      pIdx = q2 + 1;
    }

    if(partCount >= 2) {
      String netName = parts[0];
      String netCode = "";
      
      for(int i = 0; i < partCount; i++) {
        if(parts[i].length() == 5 && isDigit(parts[i][0])) {
          netCode = parts[i];
          break;
        }
      }

      if(netCode.length() > 0) {
        foundAny = true;
        html += "<option value='" + netCode + "'>" + htmlEscape(netName) + " (" + netCode + ")</option>";
      }
    }
  }

  if(!foundAny) {
    html += "<option value=''>Nem található értelmezhető hálózat</option>";
  }

  html += "</select>";
  html += "<button style='margin-top:6px' " + String(foundAny ? "" : "disabled") + ">Kiválasztott hálózat rögzítése</button>";
  html += "</form>";

  html += "<details style='margin-top:20px'><summary class='hint' style='cursor:pointer'>Nyers modem válasz</summary>";
  html += "<div class='diag' style='margin-top:6px'>" + htmlEscape(rawRes) + "</div></details>";

  html += "<a href='/gsm'><button class='sec' style='margin-top:14px'>Vissza a GSM oldalra</button></a></div>";
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleNetManual() {
  if(sendModemBusyPage("Kézi Hálózat", "2", "/gsm")) return;
  if(!server.hasArg("netcode")) {
    server.sendHeader("Location", "/gsm");
    server.send(302);
    return;
  }
  String code = server.arg("netcode");
  code.trim();
  
  String err = setManualNetwork(code, 7); 
  String html = htmlHead("Hálózat rögzítés", "2");
  html += "<h1>Kézi hálózat rögzítése</h1>";
  
  if(err.length() == 0) {
    diagAdd("Sikeresen rögzítve a kézi hálózat: " + code);
    html += "<div class='msg ok'>A hálózat sikeresen rögzítve: " + htmlEscape(code) + "</div>";
  } else {
    diagAdd("Hiba a hálózat rögzítésekor: " + err);
    html += "<div class='msg err'>" + htmlEscape(err) + "</div>";
  }
  
  html += "<a href='/gsm'><button class='sec'>Vissza a GSM oldalra</button></a>";
  html += htmlFoot();
  server.send(200, "text/html", html);
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

