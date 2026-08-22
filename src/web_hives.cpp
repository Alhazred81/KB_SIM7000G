//web_hives.cpp 

#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "web_hives.h"
#include "web_common.h"
#include "gnss_mgr.h"
#include "modem_mgr.h"

extern WebServer server;
extern bool checkPinGuard();
extern GnssState gGnss;
extern ModemState gModem;

// --- API végpont a térképhez és a telemetriához ---
void handleMapStatusApi() {
  String json = "{";
  json += "\"lat\":" + String(gGnss.lat, 6) + ",";
  json += "\"lon\":" + String(gGnss.lon, 6) + ",";
  json += "\"fix\":" + String(gGnss.fix ? "true" : "false") + ",";
  json += "\"sat\":" + String(gGnss.satUsed) + ",";
  json += "\"signal\":" + String(gModem.signalQuality) + ",";
  json += "\"operator\":\"" + jsEscape(gModem.operatorName) + "\",";
  json += "\"netType\":\"" + gModem.netType + "\",";
  json += "\"uptime\":" + String(millis() / 60000) + ",";
  json += "\"heap\":" + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"battery\":3.95,";
  json += "\"windSpeed\":12.4";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// --- Szerver & Időjárás-állomás Infó Kártya ---
String getSzerverMapCardHtml() {
  String html = "";
  html += "<div class='card wide'>";
  html += "<h2>📡 Szerver & Időjárás-állomás Telemetria</h2>";
  html += "<div style='display:grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap:10px; margin-bottom:15px;'>";
  html += "<div style='background:#0a0a18; padding:10px; border-radius:8px; border:1px solid var(--border);'><span style='font-size:11px; color:var(--txt2);'>Térerő</span><br><b id='map_signal'>-</b></div>";
  html += "<div style='background:#0a0a18; padding:10px; border-radius:8px; border:1px solid var(--border);'><span style='font-size:11px; color:var(--txt2);'>GPS Fix / Sat</span><br><b id='map_fix'>-</b></div>";
  html += "<div style='background:#0a0a18; padding:10px; border-radius:8px; border:1px solid var(--border);'><span style='font-size:11px; color:var(--txt2);'>Uptime</span><br><b id='map_uptime'>-</b></div>";
  html += "<div style='background:#0a0a18; padding:10px; border-radius:8px; border:1px solid var(--border);'><span style='font-size:11px; color:var(--txt2);'>Memória</span><br><b id='map_heap'>-</b></div>";
  html += "</div>";
  html += "</div>";
  return html;
}

// --- Térképes / Főoldali kaptár nézet ---
// --- Térképes / Főoldali kaptár nézet ---
void handleHives() {
  if (!checkPinGuard()) return;
  String html = htmlHead("Kaptárak", "9");

  html += "<style>"
          "@keyframes flammenwerfer { 0% { opacity: 1; background-color: rgba(255,0,0,0.3); } 50% { opacity: 0.4; background-color: rgba(255,0,0,0.8); } 100% { opacity: 1; background-color: rgba(255,0,0,0.3); } }"
          "@keyframes mapIconPulse { 0% { transform: scale(1); filter: drop-shadow(0 0 2px rgba(255,0,0,0.8)); } 50% { transform: scale(1.25); filter: drop-shadow(0 0 12px rgba(255,0,0,1)); } 100% { transform: scale(1); filter: drop-shadow(0 0 2px rgba(255,0,0,0.8)); } }"
          ".hive-pulse { animation: mapIconPulse 0.8s infinite ease-in-out; transform-origin: center; }"
          ".hive-table-container { max-height: 450px; overflow-y: auto; border: 1px solid var(--border); border-radius: 8px; }"
          "table.hive-table { width: 100%; border-collapse: collapse; text-align: left; font-size: 13px; }"
          "table.hive-table th { position: sticky; top: 0; background: var(--nav); color: var(--txt); padding: 10px; border-bottom: 2px solid var(--border); z-index: 2; }"
          "table.hive-table td { padding: 10px; border-bottom: 1px solid var(--border); }"
          ".badge { padding: 4px 8px; border-radius: 4px; font-weight: bold; font-size: 11px; display: inline-block; }"
          ".b-ok { background: rgba(0,204,102,0.2); color: var(--ok); }"
          ".b-yell { background: rgba(255,255,0,0.2); color: #e6e600; }"
          ".b-org { background: rgba(255,165,0,0.2); color: #ffa500; }"
          ".b-red { background: rgba(255,0,0,0.2); color: #ff3333; }"
          ".b-cyc { background: rgba(255,0,255,0.2); color: #ff00ff; }"
          ".b-flame { background: rgba(255,0,0,0.5); color: #fff; animation: flammenwerfer 0.8s infinite; }"
          ".dim { color: var(--txt2); font-size: 12px; }"
          ".custom-hive-icon { background: transparent; border: none; }"
          "</style>";

  html += "<div class='card wide' style='grid-column:1/-1'>"
          "<h2>🗺 Kaptárak & Időjárás-állomás Térképes Áttekintése</h2>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<div id='hiveMap' style='height:380px;border-radius:8px;margin-top:6px;z-index:1'></div>"
          "<script>"
          "var osmLayerH = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {maxZoom: 19, attribution: '© OpenStreetMap'});"
          "var satLayerH = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {maxZoom: 19, attribution: 'Tiles &copy; Esri'});"
          "var hiveMap = L.map('hiveMap', {center: [47.514600, 19.043500], zoom: 19, layers: [satLayerH]});" // Magasabb zoom, alapból műhold
          "var baseLayersH = {'Utca': osmLayerH, 'Műhold': satLayerH};"
          "L.control.layers(baseLayersH).addTo(hiveMap);"
          
          // Szerver ikon
          "var weatherIcon = L.divIcon({"
          "className: 'custom-hive-icon',"
          "html: '<div style=\"background:#141428; border:2px solid var(--accent); border-radius:50%; width:40px; height:40px; display:flex; align-items:center; justify-content:center; box-shadow:0 4px 10px rgba(0,0,0,0.6);\" title=\"Időjárás-állomás & Szerver\"><svg viewBox=\"0 0 24 24\" width=\"22\" height=\"22\" fill=\"none\" stroke=\"var(--txt)\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z\"></path></svg></div>',"
          "iconSize: [40, 40], iconAnchor: [20, 20], popupAnchor: [0, -20]"
          "});"
          "var szerverMarker = L.marker([47.5146, 19.0435], {icon: weatherIcon, zIndexOffset: 1000}).addTo(hiveMap);"
          
          // Kaptár ikonok
          "var hiveIcon = L.divIcon({ className: 'custom-hive-icon', html: '<div style=\"background:#1f2937; border:2px solid var(--warn); border-radius:6px; width:28px; height:28px; display:flex; align-items:center; justify-content:center; box-shadow:0 2px 6px rgba(0,0,0,0.8);\"><span style=\"font-size:14px;\">🐝</span></div>', iconSize: [28,28], iconAnchor: [14,14], popupAnchor: [0,-14] });"
          "var hansIcon = L.divIcon({ className: 'custom-hive-icon', html: '<div style=\"background:rgba(255,0,0,0.6); border:2px solid #ff3333; border-radius:6px; width:28px; height:28px; display:flex; align-items:center; justify-content:center; box-shadow:0 0 10px red; animation: flammenwerfer 0.8s infinite;\"><span style=\"font-size:14px;\">🔥</span></div>', iconSize: [28,28], iconAnchor: [14,14], popupAnchor: [0,-14] });"

          // Kaptárak lehelyezése
          "var hiveA = L.marker([47.51465, 19.04355], {icon: hiveIcon}).addTo(hiveMap).bindPopup('<b>A1B2</b><br>Rendben <br><a href=\"/hive?hive=A1B2\">Kaptár nézet megnyitása</a>');"
          "var hiveC = L.marker([47.51455, 19.04345], {icon: hiveIcon}).addTo(hiveMap).bindPopup('<b>C3D4</b><br>Ellenőrzés <br><a href=\"/hive?hive=C3D4\">Kaptár nézet megnyitása</a>');"
          "var hiveE = L.marker([47.51462, 19.04342], {icon: hiveIcon}).addTo(hiveMap).bindPopup('<b>E5F6</b><br>Etetés <br><a href=\"/hive?hive=E5F6\">Kaptár nézet megnyitása</a>');"
          "var hiveG = L.marker([47.51458, 19.04358], {icon: hiveIcon}).addTo(hiveMap).bindPopup('<b>G7H8</b><br>Atkakezelés <br><a href=\"/hive?hive=G7H8\">Kaptár nézet megnyitása</a>');"
          "var hiveDEAD = L.marker([47.51468, 19.04348], {icon: hansIcon}).addTo(hiveMap).bindPopup('<b>DEAD</b><br>Kritikus 🔥 <br><a href=\"/hive?hive=DEAD\">Kaptár nézet megnyitása</a>');"

          "function updateSzerverMapLive() {"
          "fetch('/api/map_status')"
          ".then(r => r.json())"
          ".then(d => {"
          "let sigColor = d.signal > -85 ? '#22c55e' : (d.signal > -100 ? '#eab308' : '#ef4444');"
          "let batColor = d.battery >= 3.8 ? '#22c55e' : (d.battery >= 3.5 ? '#eab308' : '#ef4444');"
          "let windColor = d.windSpeed <= 25 ? '#22c55e' : (d.windSpeed <= 45 ? '#eab308' : '#ef4444');"
          "document.getElementById('map_signal').innerHTML = '<span style=\"color:' + sigColor + ';\">📶 ' + d.signal + ' dBm</span>';"
          "document.getElementById('map_fix').innerHTML = d.fix ? ('<span style=\"color:#22c55e\">Van (' + d.sat + ')</span>') : '<span style=\"color:#ef4444\">Nincs</span>';"
          "document.getElementById('map_uptime').innerText = d.uptime + ' perc';"
          "document.getElementById('map_heap').innerText = d.heap + ' KB';"
          
          "let popupHtml = '<div style=\"font-family:sans-serif; font-size:12px; color:#111; min-width:190px;\">' +"
          "'<b style=\"font-size:13px; color:#0055ff;\">📡 Időjárás-állomás & Szerver</b><hr style=\"margin:4px 0;\">' +"
          "'<b>GPS Fix:</b> ' + (d.fix ? ('<span style=\"color:green\">Van (' + d.sat + ' sat)</span>') : '<span style=\"color:red\">Nincs</span>') + '<br>' +"
          "'<b>GSM Térerő:</b> <span style=\"color:' + sigColor + '; font-weight:bold;\">' + d.signal + ' dBm</span><br>' +"
          "'<b>Akkumulátor:</b> <span style=\"color:' + batColor + '; font-weight:bold;\">' + (d.battery || '3.95') + ' V</span><br>' +"
          "'<b>Szélerősség:</b> <span style=\"color:' + windColor + '; font-weight:bold;\">' + (d.windSpeed || '0.0') + ' km/h</span><br>' +"
          "'<b>Operátor:</b> ' + (d.operator || 'Ismeretlen') + '<br>' +"
          "'<b>Uptime:</b> ' + d.uptime + ' perc' + '</div>';"
          "szerverMarker.bindPopup(popupHtml);"
          
          "if(d.lat && d.lon && (d.lat !== 0 || d.lon !== 0)) {"
          "  let lat = d.lat; let lon = d.lon;"
          "  szerverMarker.setLatLng([lat, lon]);"
          // A kaptárak dinamikusan eltolva követik a bázis állomást (mintha körülötte állnának)
          "  hiveA.setLatLng([lat + 0.00005, lon + 0.00005]);"
          "  hiveC.setLatLng([lat - 0.00005, lon - 0.00005]);"
          "  hiveE.setLatLng([lat + 0.00002, lon - 0.00008]);"
          "  hiveG.setLatLng([lat - 0.00002, lon + 0.00008]);"
          "  hiveDEAD.setLatLng([lat + 0.00008, lon - 0.00002]);"
          // Csak egyszer centrizzük a térképet, hogy ne ugráljon idegesítően, miközben nézegeted
          "  if(!window.mapCentered) { hiveMap.setView([lat, lon], 19); window.mapCentered = true; }"
          "}"
          "}).catch(()=>{});"
          "}"
          "setInterval(updateSzerverMapLive, 5000);"
          "updateSzerverMapLive();"
          "</script></div>";

  html += getSzerverMapCardHtml();

  html += "<div class='card wide'><h2>Állapot és Beavatkozási Ütemterv</h2>";
  html += "<p class='hint'>Sárga: 3 napon túl | Narancs: 3 napon belül | Piros: Holnap | Ciklámen: Ma | 🔥 Hans: Kritikus.</p>";
  html += "<div class='hive-table-container'>";
  html += "<table class='hive-table'>";
  html += "<thead><tr><th>Azonosító</th><th>Család állapota</th><th>Monitor állapota</th><th>Beavatkozás</th></tr></thead>";
  html += "<tbody>";
  // A táblázatban is bekötöttem a linkeket, hogy kattinthatók legyenek!
  html += "<tr><td><a href='/hive?hive=A1B2' style='color:var(--accent); font-weight:bold;'>A1B2</a></td><td>Rendben</td><td>OK</td><td><span class='badge b-yell'>5 nap múlva</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=C3D4' style='color:var(--accent); font-weight:bold;'>C3D4</a></td><td>Ellenőrzés</td><td>Jelerősség gyenge</td><td><span class='badge b-org'>2 nap múlva</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=E5F6' style='color:var(--accent); font-weight:bold;'>E5F6</a></td><td>Etetés</td><td><span style='color:var(--err)'>Alacsony akku (10%)</span></td><td><span class='badge b-red'>Holnap</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=G7H8' style='color:var(--accent); font-weight:bold;'>G7H8</a></td><td>Atkakezelés</td><td><span style='color:var(--err)'>Szenzor olvasási hiba</span></td><td><span class='badge b-cyc'>Ma (Azonnal)</span></td></tr>";
  html += "<tr><td><a href='/hive?hive=DEAD' style='color:var(--accent); font-weight:bold;'>DEAD</a></td><td><span class='badge b-flame'>🔥 Hans</span></td><td>OFFLINE</td><td><span class='badge b-flame'>🔥 Hans</span></td></tr>";
  html += "</tbody></table></div></div>";
  
  html += htmlFoot();
  server.send(200, "text/html", html);
}

// --- Értékelő oldal (-- / ++ skálákkal) ---
void handleEvaluate() {
  String hiveId = server.hasArg("hive") ? server.arg("hive") : "Ismeretlen";

  String html = htmlHead("Értékelés: " + hiveId, "0");
  
  html += "<style>"
          ".scale-group { display: flex; width: 100%; border: 1px solid var(--border); border-radius: 6px; overflow: hidden; margin-bottom: 20px; }"
          ".scale-group input[type='radio'] { display: none; }"
          ".scale-group label { flex: 1; text-align: center; padding: 12px 0; font-size: 16px; font-weight: bold; color: var(--txt); background: rgba(0,0,0,0.1); border-right: 1px solid var(--border); cursor: pointer; }"
          ".scale-group label:last-child { border-right: none; }"
          ".scale-group input[type='radio']:checked + label { background: var(--prim); color: #fff; }"
          ".metric-title { font-weight: bold; margin-bottom: 5px; display: block; font-size: 14px; color: var(--txt); }"
          "</style>";

  html += "<h2>Kaptár: <span style='color:var(--prim)'>" + hiveId + "</span></h2>";
  html += "<form action='/evaluate_post' method='POST'>";
  html += "<input type='hidden' name='hive' value='" + hiveId + "'>";

  auto makeScale = [](const String& name, const String& label) {
    String s = "<span class='metric-title'>" + label + "</span>";
    s += "<div class='scale-group'>";
    String vals[] = {"--", "-", "0", "+", "++"};
    String keys[] = {"-2", "-1", "0", "1", "2"};
    for(int i=0; i<5; i++) {
      String id = name + keys[i];
      String checked = (i == 2) ? "checked" : "";
      s += "<input type='radio' name='" + name + "' id='" + id + "' value='" + keys[i] + "' " + checked + ">";
      s += "<label for='" + id + "'>" + vals[i] + "</label>";
    }
    s += "</div>";
    return s;
  };

  html += "<div class='card'>";
  html += "<h3>Szubjektív megfigyelések</h3>";
  html += makeScale("gentle", "Szelídség (Lépen ülés, nyugodtság)");
  html += makeScale("swarm", "Rajzási hajlam (- : azonnal rajzik, + : nem akar)");
  html += makeScale("spring", "Tavaszi fejlődés erélye");
  html += makeScale("vsh", "Tisztító ösztön (Higiénikus viselkedés)");
  html += makeScale("brood", "Fiasítás kiterjedése és minősége");
  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>Objektív adatok</h3>";
  html += "<label>Méz hozam (fiók / becsült kg)</label>";
  html += "<input type='number' step='0.5' name='yield' placeholder='pl. 1.5' style='width:100%; margin-bottom:15px;'>";
  html += "<label>Atkahullás (kontroll utáni db)</label>";
  html += "<input type='number' name='mites' placeholder='pl. 42' style='width:100%; margin-bottom:15px;'>";
  html += "<label>Szirup etetés (liter)</label>";
  html += "<input type='number' step='0.1' name='syrup' placeholder='pl. 2.5' style='width:100%; margin-bottom:15px;'>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>Döntés / Státusz</h3>";
  html += "<select name='status' style='width:100%; font-size:16px; padding:10px;'>";
  html += "<option value='termelo' selected>Termelő (Normál)</option>";
  html += "<option value='tenyesz'>Tenyész (Anya nevelésre)</option>";
  html += "<option value='dajka'>Dajka / Starter</option>";
  html += "<option value='anyanevelesi_mesterterv'>Anyanevelési Mesterterv</option>";
  html += "<option value='petes_anyanevelesi_mesterterv'>Petés Anyanevelési Mesterterv</option>";
  html += "<option value='teli_egyesites'>Téli egyesítés (Gyenge)</option>";
  html += "<option value='hans'>Hans 🔥 (Kuka/Leváltás)</option>";
  html += "</select>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<label>Egyéb megjegyzés (max 200 kar.)</label>";
  html += "<textarea name='notes' rows='3' style='width:100%; font-family:inherit;'></textarea>";
  html += "</div>";

  html += "<button type='submit' class='warn' style='width:100%; font-size:18px; padding:15px; margin-bottom:20px;'>💾 Értékelés Mentése</button>";
  html += "</form>";
  html += htmlFoot();
  
  server.send(200, "text/html", html);
}

void handleEvaluatePost() {
  String hive = server.hasArg("hive") ? server.arg("hive") : "Ismeretlen";
  String gentle = server.hasArg("gentle") ? server.arg("gentle") : "0";
  String status = server.hasArg("status") ? server.arg("status") : "termelo";
  String syrup = server.hasArg("syrup") && server.arg("syrup") != "" ? server.arg("syrup") : "0";
  
  String logMsg = "Értékelés mentve [" + hive + "] Status: " + status + ", Szelídség: " + gentle + ", Szirup: " + syrup + " L";

  server.sendHeader("Location", "/");
  server.send(302);
}

// --- Konfigurátor & NFC fiók-tanítás ---
void handleConfig() {
  String hiveMac = server.hasArg("hive") ? server.arg("hive") : "unknown";
  
  String html = htmlHead("Kaptár Konfig: " + hiveMac, "0");
  
  html += "<div class='card'>";
  html += "<h2>Fiók-konfiguráció</h2>";
  html += "<p>MAC: <b>" + hiveMac + "</b></p>";
  html += "<form action='/config_post' method='POST'>";
  html += "<input type='hidden' name='hive' value='" + hiveMac + "'>";

  html += "<label>Teljes fiókok száma:</label>";
  html += "<input type='number' name='full_supers' value='2' min='0' style='width:100%; margin-bottom:10px;'>";
  html += "<label>Félfiókok száma:</label>";
  html += "<input type='number' name='half_supers' value='4' min='0' style='width:100%; margin-bottom:10px;'>";

  html += "<div id='nfcScanArea' style='margin-top:20px; padding:15px; border:2px dashed var(--border); text-align:center;'>";
  html += "<h3>Fiókok NFC hozzárendelése</h3>";
  html += "<button type='button' id='scanFio' class='sec'>➕ Új fiók tag hozzárendelése</button>";
  html += "<ul id='fioList' style='text-align:left; margin-top:10px;'></ul>";
  html += "</div>";

  html += "<button type='submit' class='ok' style='width:100%; margin-top:20px;'>💾 Konfiguráció Mentése</button>";
  html += "</form></div>";

  html += "<script>"
          "document.getElementById('scanFio').addEventListener('click', async () => {"
          "  const ndef = new NDEFReader(); await ndef.scan();"
          "  ndef.onreading = event => {"
          "    for (const record of event.message.records) {"
          "      const tagId = new TextDecoder().decode(record.data);"
          "      const list = document.getElementById('fioList');"
          "      list.innerHTML += '<li>Fiók Tag: ' + tagId + '<input type=\"hidden\" name=\"fio_tags\" value=\"' + tagId + '\"></li>';"
          "    }"
          "  };"
          "});"
          "</script>";
  
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleConfigPost() {
  String hiveMac = server.arg("hive");
  Preferences prefs;
  prefs.begin(hiveMac.c_str(), false);
  prefs.putInt("full", server.arg("full_supers").toInt());
  prefs.putInt("half", server.arg("half_supers").toInt());
  prefs.end();

  server.sendHeader("Location", "/");
  server.send(303);
}

// --- Registry (Típusok kezelése LittleFS-ből) ---
String getHiveTypesHtml() {
  File file = LittleFS.open("/hives_types.json", "r");
  if (!file) return "<option value='none'>Hiba: Nincs típusfájl!</option>";

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) return "<option value='none'>Hiba: JSON hiba!</option>";

  String html = "";
  JsonArray array = doc.as<JsonArray>();
  for (JsonObject type : array) {
    html += "<option value='" + String(type["id"].as<const char*>()) + "'>" 
          + String(type["name"].as<const char*>()) + "</option>";
  }
  return html;
}

void handleRegisterPart() {
  String html = htmlHead("Eszköz regisztráció", "0");
  html += "<div class='card'><h2>Elem regisztrálása</h2>";
  html += "<form action='/register_part_post' method='POST'>";
  
  html += "<input type='hidden' name='tag_id' id='tag_id'>";
  
  html += "<label>Kaptárelem típusa:</label>";
  html += "<select name='type' style='width:100%; padding:10px; margin-bottom:15px;'>";
  html += getHiveTypesHtml();
  html += "</select>";
  
  html += "<button type='button' id='scanBtn' class='sec' style='width:100%'>📱 NFC Tag beolvasása</button>";
  html += "<p id='scanStatus' style='color:var(--prim); margin-top:10px;'></p>";
  
  html += "<button type='submit' class='ok' style='width:100%; margin-top:20px;'>💾 Regisztrálás</button>";
  html += "</form></div>";

  html += "<script>"
          "document.getElementById('scanBtn').addEventListener('click', async () => {"
          "  const ndef = new NDEFReader(); await ndef.scan();"
          "  ndef.onreading = event => {"
          "    for (const record of event.message.records) {"
          "      const tagId = new TextDecoder().decode(record.data);"
          "      document.getElementById('tag_id').value = tagId;"
          "      document.getElementById('scanStatus').innerText = '✅ Tag beolvasva: ' + tagId;"
          "    }"
          "  };"
          "});"
          "</script>";
          
  html += htmlFoot();
  server.send(200, "text/html", html);
}

void handleRegisterPartPost() {
  String tagId = server.hasArg("tag_id") ? server.arg("tag_id") : "";
  String typeId = server.hasArg("type") ? server.arg("type") : "unknown";

  if (tagId.length() > 0) {
    Preferences prefs;
    prefs.begin("hive_parts", false);
    prefs.putString(tagId.c_str(), typeId.c_str());
    prefs.end();
  }

  server.sendHeader("Location", "/");
  server.send(303);
}