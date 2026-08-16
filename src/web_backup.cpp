//web_backup.cpp

#include "web_backup.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <WebServer.h>
#include "config.h"
#include "web_common.h"
#include "web_theme.h"

extern WebServer server;

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
    html += "/diag<button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  int declaredSize = data.substring(0, c1).toInt();
  uint32_t declaredChecksum = (uint32_t)data.substring(c1 + 1, c2).toInt();
  String b64 = data.substring(c2 + 1);

  if(declaredSize != EEPROM_SIZE) {
    html += "<div class='msg err'>Meret-eltero export (" + String(declaredSize) + " byte).</div>";
    html += "/diag<button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  uint8_t buf[EEPROM_SIZE];
  size_t decoded = base64Decode(b64, buf, EEPROM_SIZE);

  if(decoded != (size_t)EEPROM_SIZE) {
    html += "<div class='msg err'>Hianyos/serult adat.</div>";
    html += "/diag<button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  uint32_t checksum = 0;
  for(int i = 0; i < EEPROM_SIZE; i++) {
    checksum = (checksum * 31) + buf[i];
  }

  if(checksum != declaredChecksum) {
    html += "<div class='msg err'>Ellenorzo osszeg hiba - az adat serult.</div>";
    html += "/diag<button class='sec'>Vissza</button></a>";
    html += htmlFoot();
    server.send(200, "text/html", html);
    return;
  }

  for(int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, buf[i]);
  }

  EEPROM.commit();

  diagAdd("EEPROM visszatoltve (manualis).");

  html += "<div class='msg ok'>Beallitasok sikeresen visszatoltve!</div>";html += "/reinit";
  html += "<button class='warn'>Modem ujraindit</button></form>";
  html += "/reinit<button class='warn'>Modem ujraindit</button></form>";
  html += "/<button class='sec' style='margin-top:8px'>Fooldal</button></a>";
  html += htmlFoot();

  server.send(200, "text/html", html);
}