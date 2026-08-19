#include "web_sensors.h"
#include "sensors.h"
#include "web_common.h"
#include <WebServer.h>

// --- Globális változók és extern deklarációk ---
extern WebServer server;
extern WindSpeedState gWindSpeed;
extern WindDirState gWindDir;
extern ShtSensorState gSht;
extern RainSensorState gRain;
extern Mpu6050State gMpu;
extern Aht20Bmp280State gAhtBmp;
extern Ltr390State gLtr;
extern uint32_t gSensRs485Baud;
extern bool gRs485Initialized;
extern String gLastSensTestResult;
extern bool gLastSensTestOk;
extern String gLastSensTestRaw;

// --- Külső függvények a web_ui.cpp-ből ---
extern bool checkPinGuard();
extern void sensSetEnabled(uint8_t bit, bool on);
extern void sensorsApplyEnabled();
extern void saveSensorConfig();
extern void rs485Init();
extern String compassAbbrev(int deg);

// --- Segédfüggvények ---

String sensorRowHtml(const String& sensorKey, const String& label, bool enabled, bool hasEverRead, bool isOk, const String& valueText, const String& pinInfo) {
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

String sensStatusJsonEntry(const String& key, bool enabled, bool hasEverRead, bool isOk, const String& value) {
    return "\"" + key + "\":{\"enabled\":" + String(enabled?"true":"false") + 
           ",\"hasEverRead\":" + String(hasEverRead?"true":"false") + 
           ",\"ok\":" + String(isOk?"true":"false") + 
           ",\"value\":\"" + value + "\"}";
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

// --- Handler-ek ---

void handleSensors() {
    if (!checkPinGuard()) return;
    String html = htmlHead("Szenzorok", "7");

    html += "<div class='card wide'><h2>Allapot</h2>";
    html += sensorRowHtml("windspeed", "Szelsebesseg", gWindSpeed.enabled, gWindSpeed.lastGoodRead>0, gWindSpeed.lastReadOk, windSpeedValueText());
    html += sensorRowHtml("winddir", "Szelirany", gWindDir.enabled, gWindDir.lastGoodRead>0, gWindDir.lastReadOk, windDirValueText());
    html += sensorRowHtml("sht", "SHT57 ho/para", gSht.enabled, gSht.lastGoodRead>0, gSht.lastReadOk, shtValueText());
    html += sensorRowHtml("rain", "Esoszenzor", gRain.enabled, gRain.lastPoll>0, true, rainValueText());
    html += sensorRowHtml("mpu", "MPU6050 (I2C1)", gMpu.enabled, gMpu.lastGoodRead>0, gMpu.lastReadOk, mpuValueText());
    html += sensorRowHtml("ahtbmp", "AHT20+BMP280 (I2C2)", gAhtBmp.enabled, gAhtBmp.lastGoodRead>0, gAhtBmp.lastReadOk, ahtBmpValueText());
    html += sensorRowHtml("ltr", "LTR-390 UV (I2C2)", gLtr.enabled, gLtr.lastGoodRead>0, gLtr.lastReadOk, ltrValueText());
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

void handleSensTest() {
    if(!server.hasArg("which")) {
        server.sendHeader("Location","/sensors"); server.send(302); return;
    }
    sensTestRun(server.arg("which"));
    diagAdd("Szenzor teszt (" + server.arg("which") + "): " + gLastSensTestResult);
    server.sendHeader("Location","/sensors");
    server.send(302);
}