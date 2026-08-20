#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "web_hives.h"
#include "web_common.h"

extern WebServer server;
extern bool checkPinGuard();

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
          "<h2>🗺 Kaptárak Térképes Áttekintése</h2>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<div id='hiveMap' style='height:350px;border-radius:8px;margin-top:6px;z-index:1'></div>"
          "<script>"
          "var osmLayerH = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {maxZoom: 19, attribution: '© OpenStreetMap'});"
          "var satLayerH = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {maxZoom: 19, attribution: 'Tiles &copy; Esri'});"
          "var hiveMap = L.map('hiveMap', {center: [47.514600, 19.043500], zoom: 15, layers: [osmLayerH]});"
          "var baseLayersH = {'Utca': osmLayerH, 'Műhold': satLayerH};"
          "L.control.layers(baseLayersH).addTo(hiveMap);"
          "function getQueenColor(year) {"
          "  var lastDigit = year % 10;"
          "  if (lastDigit === 1 || lastDigit === 6) return '#FFFFFF';"
          "  if (lastDigit === 2 || lastDigit === 7) return '#FFFF00';"
          "  if (lastDigit === 3 || lastDigit === 8) return '#FF0000';"
          "  if (lastDigit === 4 || lastDigit === 9) return '#00FF00';"
          "  return '#0000FF';"
          "}"
          "function getInterventionColor(days) {"
          "  if (days === 'hans') return '#FF0000';"
          "  if (days > 3) return '#FFFF00';"
          "  if (days > 1) return '#FFA500';"
          "  if (days === 1) return '#FF0000';"
          "  if (days === 0) return '#FF00FF';"
          "  return '#00CC66';"
          "}"
          "function createSquareHiveIcon(days, queenYear, isCritical, hasSensorErr, batPct) {"
          "  var qColor = getQueenColor(queenYear);"
          "  var statusColor = getInterventionColor(days);"
          "  var pulseClass = isCritical ? ' hive-pulse' : '';"
          "  var errSvg = hasSensorErr ? '<circle cx=\"24\" cy=\"24\" r=\"9\" fill=\"#FF0000\" stroke=\"#2a2a40\" stroke-width=\"2\"/><text x=\"24\" y=\"28\" fill=\"white\" font-size=\"11\" font-family=\"sans-serif\" font-weight=\"bold\" text-anchor=\"middle\">!</text>' : '';"
          "  var batWidth = (batPct / 100) * 20;"
          "  var batColor = batPct > 20 ? '#00FF00' : '#FF0000';"
          "  var batSvg = '<rect x=\"40\" y=\"68\" width=\"20\" height=\"8\" fill=\"#333\" stroke=\"#2a2a40\" stroke-width=\"1.5\" rx=\"1\"/><rect x=\"40\" y=\"68\" width=\"' + batWidth + '\" height=\"8\" fill=\"' + batColor + '\" rx=\"1\"/><rect x=\"60\" y=\"70\" width=\"2\" height=\"4\" fill=\"#2a2a40\"/>';"
          "  var svg = '<svg class=\"' + pulseClass.trim() + '\" viewBox=\"0 0 100 100\" xmlns=\"http://www.w3.org/2000/svg\">' +"
          "    '<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" fill=\"' + statusColor + '\" stroke=\"#2a2a40\" stroke-width=\"6\" rx=\"12\"/>' +"
          "    '<circle cx=\"50\" cy=\"44\" r=\"14\" fill=\"' + qColor + '\" stroke=\"#2a2a40\" stroke-width=\"3\"/>' +"
          "    errSvg + batSvg +"
          "  '</svg>';"
          "  return L.divIcon({ className: 'custom-hive-icon', html: svg, iconSize: [36, 36], iconAnchor: [18, 18], popupAnchor: [0, -18] });"
          "}"
          "var markers = [];"
          "var hivesData = ["
          "  {lat: 47.5146, lon: 19.0435, id: 'A1B2', famStat: 'Rendben', monStat: 'OK', days: 5, err: false, bat: 100, year: 2024, critical: false},"
          "  {lat: 47.5155, lon: 19.0412, id: 'C3D4', famStat: 'Ellenőrzés', monStat: 'Gyenge jel', days: 2, err: false, bat: 50, year: 2023, critical: false},"
          "  {lat: 47.5132, lon: 19.0458, id: 'E5F6', famStat: 'Etetés', monStat: 'Alacsony akku (10%)', days: 1, err: false, bat: 10, year: 2022, critical: false},"
          "  {lat: 47.5121, lon: 19.0405, id: 'G7H8', famStat: 'Atkakezelés', monStat: 'Szenzor hiba', days: 0, err: true, bat: 90, year: 2021, critical: false},"
          "  {lat: 47.5150, lon: 19.0495, id: 'DEAD', famStat: '🔥 Hans', monStat: 'OFFLINE', days: 'hans', err: true, bat: 0, year: 2023, critical: true}"
          "];"
          "hivesData.forEach(function(h) {"
          "  var m = L.marker([h.lat, h.lon], {icon: createSquareHiveIcon(h.days, h.year, h.critical, h.err, h.bat)}).addTo(hiveMap)"
          "    .bindPopup('<b>Kaptár: ' + h.id + '</b><br>Család: ' + h.famStat + '<br>Monitor: ' + h.monStat);"
          "  markers.push(m);"
          "});"
          "if(markers.length > 0) {"
          "  var group = L.featureGroup(markers);"
          "  hiveMap.fitBounds(group.getBounds().pad(0.2));"
          "}"
          "</script></div>";

  html += "<div class='card wide'><h2>Állapot és Beavatkozási Ütemterv</h2>";
  html += "<p class='hint'>Sárga: 3 napon túl | Narancs: 3 napon belül | Piros: Holnap | Ciklámen: Ma | 🔥 Hans: Kritikus.</p>";
  html += "<div class='hive-table-container'>";
  html += "<table class='hive-table'>";
  html += "<thead><tr><th>Azonosító</th><th>Család állapota</th><th>Monitor állapota</th><th>Beavatkozás</th></tr></thead>";
  html += "<tbody>";
  html += "<tr><td>A1B2</td><td>Rendben</td><td>OK</td><td><span class='badge b-yell'>5 nap múlva</span></td></tr>";
  html += "<tr><td>C3D4</td><td>Ellenőrzés</td><td>Jelerősség gyenge</td><td><span class='badge b-org'>2 nap múlva</span></td></tr>";
  html += "<tr><td>E5F6</td><td>Etetés</td><td><span style='color:var(--err)'>Alacsony akku (10%)</span></td><td><span class='badge b-red'>Holnap</span></td></tr>";
  html += "<tr><td>G7H8</td><td>Atkakezelés</td><td><span style='color:var(--err)'>Szenzor olvasási hiba</span></td><td><span class='badge b-cyc'>Ma (Azonnal)</span></td></tr>";
  html += "<tr><td>DEAD</td><td><span class='badge b-flame'>🔥 Hans</span></td><td>OFFLINE</td><td><span class='badge b-flame'>🔥 Hans</span></td></tr>";
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
  diagAdd(logMsg);

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