//web_config.cpp

#include "web_config.h"
#include "web_common.h"
#include "config.h"
#include "modem_mgr.h"
#include "wifi_sta.h"
#include <WebServer.h>
#include <EEPROM.h>

extern WebServer server;
extern WifiStaState gSta;
extern ModemState gModem;
extern LedConfig gLed;
extern String gApSSID;
extern String gApPass;
extern uint8_t gApChannel;
extern ScannedNet gScanResults[];
extern int gScanCount;
extern bool gModemInitRequested;

// Külső függvények, amiket a config használ
extern void wifiScan();
extern void wifiStaConnect(const String& ssid, const String& pass);
extern void wifiStaDisconnect();
extern void saveApPass(const String& pass);
extern void savePin(const String& pin);
extern String changeSIMPin(const String& oldPin, const String& newPin);
extern void saveLedConfig();
extern void ledPinReinit();
extern int currentLedGpio();
extern void setNetLightAT(bool on);
extern void ledTrigger();
extern void ledSetAuto();
extern void sendWaitPage(const String& title, const String& message, const String& nextUrl, int waitSeconds);

// A PIN teszteléshez szükséges ideiglenes változó (ez a web_ui.cpp-ből jön át!)
static String gLastValidPin = "";

// --- IDE JÖNNEK A FÜGGVÉNYEK A web_ui.cpp-BŐL ---

void handleCfg() {
  String html = htmlHead("Beallitasok", "4");

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
          "<label>Eszközazonosító (Név, pl. szerver-1)</label>"
          "<input type='text' name='ntfy_nickname' value='" + gNtfyNickname + "'>"
          
          "<div style='display:flex; align-items:center; justify-content:space-between; margin-top:15px; padding-top:10px; border-top:1px solid var(--border);'>"
          "<span>Rendszerindulási tesztüzenet</span>"
          "<label class='sens-toggle' style='--sens-color:var(--ok); margin:0;'>"
          "<input type='checkbox' name='ntfy_startup'" + String(gNtfyStartupMsg ? " checked" : "") + ">"
          "<span class='slider'></span></label>"
          "</div>"
          
          "<button style='margin-top:20px'>ntfy Mentés</button>"
          "</form></div>";

  if (server.hasArg("pin_ok") && gLastValidPin.length() > 0) {
    html += "<div class='card wide' style='border-color:var(--ok);'>"
           "<h2>🎉 SIM sikeresen feloldva!</h2>"
           "<p class='hint'>A megadott PIN kód helyesnek bizonyult. Szeretnéd XTEA-val titkosítva elmenteni, hogy a jövőben automatikusan csatlakozzon?</p>"
           "<form action='/confirmsavepin' method='POST'>"
           "<input type='hidden' name='confirmed_pin' value='" + gLastValidPin + "'>"
           "<button style='background:var(--ok); margin-top:10px;'>Igen, mentés XTEA titkosítással</button>"
           "</form></div>";
  } else if (server.hasArg("pin_err")) {
    html += "<div class='card wide' style='border-color:var(--err);'>"
           "<h2>❌ Hibás PIN kód</h2>"
           "<p class='hint' style='color:var(--err);'>A megadott PIN kóddal a SIM kártya elutasította a bejelentkezést.</p></div>";
  }

  html += "<div class='card'><h2>SIM PIN teszt & mentés</h2>"
          "<form action='/testsavepin' method='POST'>"
          "<label>PIN kód (4-8 szám)</label>"
          "<input type='password' name='pin' id='pi' maxlength='8' "
          "pattern='[0-9]{4,8}' placeholder='pl. 1234' oninput='pc()'>"
          "<button type='submit' id='pb' disabled>PIN tesztelése</button>"
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

void handleTestSavePin() {
  if(!server.hasArg("pin")){ server.sendHeader("Location","/cfg"); server.send(302); return; }
  String pin = server.arg("pin"); pin.trim();
  
  modem.simUnlock(pin.c_str());
  delay(1200);

  int simStat = modem.getSimStatus();
  if (simStat == 1 /* SIM_READY */) {
    gLastValidPin = pin;
    diagAdd("SIM PIN teszt SIKERES.");
    server.sendHeader("Location", "/cfg?pin_ok=1");
  } else {
    gLastValidPin = "";
    diagAdd("SIM PIN teszt SIKERTELEN. (Kód: " + String(simStat) + ")");
    server.sendHeader("Location", "/cfg?pin_err=1");
  }
  server.send(302);
}

void handleConfirmSavePin() {
  if(server.hasArg("confirmed_pin") && server.arg("confirmed_pin") == gLastValidPin && gLastValidPin.length() > 0) {
    savePin(gLastValidPin); 
    diagAdd("PIN sikeresen elmentve XTEA titkosítással.");
    gLastValidPin = "";
    gModemInitRequested = true;
    sendWaitPage("Modem Inicializálás", "A PIN kód biztonságosan elmentve. A modem újracsatlakozása folyamatban...", "/", 30);
    return;
  }
  server.sendHeader("Location", "/cfg");
  server.send(302);
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

