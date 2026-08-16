//gnss_mgr.h

#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "time_mgr.h"
#include "modem_mgr.h"

struct GnssState {
  bool    enabled     = false;
  bool    fix         = false;
  float   lat         = 0;
  float   lon         = 0;
  float   alt         = 0;
  float   speed       = 0;
  float   course      = 0;
  float   hdop        = 99.9;
  // ── +CGNSINF-bol (fix eseten ervenyes) ──────────────────────
  int     satUsed     = 0;   // OSSZES rendszer egyutt hasznalt muhold (CGNSINF mezo 15)
  int     satGpsInView= -1;  // GPS lathato muhold, fix nelkul is (CGNSINF mezo 14)
  // ── +CGNSSINFO-bol (rendszerenkenti bontas, "SVs" - hasznalt/valid) ─
  int     satGPS       = -1; // GPS hasznalt (CGNSSINFO GPS-SVs)
  int     satGLO        = -1; // GLONASS hasznalt (CGNSSINFO GLONASS-SVs)
  int     satBDS        = -1; // BEIDOU hasznalt (CGNSSINFO BEIDOU-SVs)
  int     satGAL         = -1; // nincs ilyen mezo egyik parancsban sem -> mindig n/a
  int     satTotalView   = -1; // GPS+GLONASS+BEIDOU osszege (CGNSSINFO alapjan)
  String  rawCgnsinf  = "";
  String  rawCgnssinfo= "";
  String  timeStr     = "--:--:--";
  String  dateStr     = "--.--.----";
  int     antStatus   = -1;   // -1=ismeretlen, 0=nincs/szakadt, 1=OK, 2=rovidzarlat
  int     runStatus   = 0;
  int     fixStatus   = 0;
  unsigned long startedAt = 0;
  unsigned long lastGoodFix = 0;
  unsigned long lastPoll = 0;
  unsigned long lastPositionPoll = 0;
  unsigned long lastExtendedPoll = 0;
  unsigned long lastAntennaPoll = 0;
  String  lastError    = "";
  float   assistLat    = GNSS_DEFAULT_ASSIST_LAT;
  float   assistLon    = GNSS_DEFAULT_ASSIST_LON;
};
extern GnssState gGnss;

void gnssLoadAssist() {
  if(EEPROM.read(ADDR_GNSS_ASSIST_FLAG) == MAGIC_BYTE) {
    EEPROM.get(ADDR_GNSS_ASSIST_LAT, gGnss.assistLat);
    EEPROM.get(ADDR_GNSS_ASSIST_LON, gGnss.assistLon);
    if(isnan(gGnss.assistLat) || isnan(gGnss.assistLon) ||
       gGnss.assistLat < -90 || gGnss.assistLat > 90 ||
       gGnss.assistLon < -180 || gGnss.assistLon > 180) {
      gGnss.assistLat = GNSS_DEFAULT_ASSIST_LAT;
      gGnss.assistLon = GNSS_DEFAULT_ASSIST_LON;
    }
  }
}

void gnssSaveAssist(float lat, float lon) {
  if(lat < -90 || lat > 90 || lon < -180 || lon > 180) return;
  gGnss.assistLat = lat;
  gGnss.assistLon = lon;
  EEPROM.write(ADDR_GNSS_ASSIST_FLAG, MAGIC_BYTE);
  EEPROM.put(ADDR_GNSS_ASSIST_LAT, gGnss.assistLat);
  EEPROM.put(ADDR_GNSS_ASSIST_LON, gGnss.assistLon);
  EEPROM.commit();
  Serial.println("[GNSS] Kiindulo koordinata mentve: " + String(gGnss.assistLat, 6) + ", " + String(gGnss.assistLon, 6));
}

String gnssReceiverStatusText() {
  if(!gGnss.enabled) return "Kikapcsolva";
  if(gGnss.fix) return "Bekapcsolva, fix van";
  if(gGnss.runStatus == 1) return "Bekapcsolva, keresi a fixet";
  return "Bekapcsolva, de a vevo nem fut";
}

String gnssCompassDir(float course) {
  const char* dirs[] = {"Eszak","Eszak-kelet","Kelet","Del-kelet","Del","Del-nyugat","Nyugat","Eszak-nyugat"};
  int idx = (int)((course + 22.5) / 45.0) % 8;
  if(idx < 0) idx += 8;
  return String(dirs[idx]);
}

void gnssStart() {
  modem.sendAT("+CGNSPWR=0"); modem.waitResponse(1000L); yield();
  delay(300); yield();
  modem.sendAT("+CGNSPWR=1"); modem.waitResponse(2000L); yield();
  delay(500); yield();
  modem.sendAT("+CGNSMOD=1,1,1,1");
  modem.waitResponse(1000L);
  gGnss.enabled = true;
  gGnss.startedAt = millis();
  gGnss.lastError = "";
  Serial.println(F("[GNSS] Inditva (GPS+GLONASS+BDS+GALILEO)."));
  Serial.println("[GNSS] Kiindulo koordinata: " + String(gGnss.assistLat, 6) + ", " + String(gGnss.assistLon, 6));
}

void gnssStop() {
  modem.sendAT("+CGNSPWR=0");
  modem.waitResponse(1000L);
  gGnss.enabled = false;
  gGnss.fix = false;
  Serial.println(F("[GNSS] Leallitva."));
}

// +CGNSINF valasz formatuma (SIMCom hivatalos GNSS Application Note alapjan):
// index: 0=run,1=fix,2=UTC,3=lat,4=lon,5=alt,6=speed,7=course,8=fixmode,
//        9=reserved1,10=HDOP,11=PDOP,12=VDOP,13=reserved2,
//        14=GPS Satellites in View, 15=GNSS Satellites Used (OSSZESITETT!),
//        16=GLONASS Satellites Used, 17=reserved3, 18=C/N0 max, 19=HPA, 20=VPA
// Pelda (hivatalos dokumentaciobol):
// +CGNSINF: 1,1,20171103022632.000,31.222067,121.354368,34.700,0.00,0.0,1,,1.1,1.4,0.9,,21,6,,,45,,
// FONTOS: a 15. mezo NEM tiszta GPS-hasznalt szam, hanem az OSSZES rendszer
// (GPS+GLONASS+BEIDOU egyutt) hasznalt muholdjainak osszege - korabban ezt
// tevesen "csak GPS"-kent kezeltuk, emiatt tunt ugy hogy csak GPS latszik.
void gnssPollPosition() {
  modem.sendAT("+CGNSINF");
  if(modem.waitResponse(800L, GF("+CGNSINF:")) != 1){
    modem.waitResponse(100L);
    gGnss.lastError = "CGNSINF nem adott valaszt 800 ms alatt";
    yield();
    return;
  }
  yield();
  String raw = modemSerial.readStringUntil('\n');
  raw.trim();
  gGnss.rawCgnsinf = raw;
  modem.waitResponse(100L);
  yield();

  String f[21];
  int fi = 0, prev = 0;
  for(int i=0; i<=(int)raw.length() && fi<21; i++){
    if(i==(int)raw.length() || raw[i]==','){
      f[fi++] = raw.substring(prev, i);
      prev = i+1;
    }
  }
  if(fi < 17) {
    gGnss.lastError = "Hianyos CGNSINF valasz (" + String(fi) + " mezo): " + raw;
    return;
  }

  gGnss.runStatus = f[0].toInt();
  gGnss.fixStatus = f[1].toInt();
  gGnss.fix = (gGnss.fixStatus == 1);
  gGnss.lastError = "";

  // GPS lathato (view) - ez fix nelkul is elerheto, jelzi hogy keres-e mar egyaltalan
  gGnss.satGpsInView = f[14].length() ? f[14].toInt() : -1;

  if(gGnss.fix){
    gGnss.lat    = f[3].toFloat();
    gGnss.lon    = f[4].toFloat();
    gGnss.alt    = f[5].toFloat();
    gGnss.speed  = f[6].toFloat();
    gGnss.course = f[7].toFloat();
    gGnss.hdop   = f[10].toFloat();
    // f[15] = OSSZES hasznalt muhold (minden rendszer egyutt), NEM csak GPS!
    gGnss.satUsed = f[15].length() ? f[15].toInt() : 0;
    gGnss.lastGoodFix = millis();

    // Utolso ismert pozicio automatikus mentese minden sikeres fixnel -
    // ez lesz a kiindulopont legkozelebb, felulirva az alapertelmezettet.
    gnssSaveAssist(gGnss.lat, gGnss.lon);

    String utc = f[2];
    if(utc.length() >= 14){
      gGnss.dateStr = utc.substring(6,8) + "." + utc.substring(4,6) + "." + utc.substring(0,4);
      gGnss.timeStr = utc.substring(8,10) + ":" + utc.substring(10,12) + ":" + utc.substring(12,14);
    }
  } else {
    gGnss.satUsed = 0;
  }
}

// +CGNSSINFO valasz formatuma (SIMCom hivatalos dokumentacio alapjan):
// mode,GPS-SVs,GLONASS-SVs,BEIDOU-SVs,lat,N/S,lon,E/W,date,time,alt,speed,course,PDOP,HDOP,VDOP
// Pelda: +CGNSSINFO: 2,06,03,00,3426.693019,S,15051.184731,E,170521,034216.0,46.5,0.0,0.0,1.2,0.9,0.9
// FONTOS: nincs kulon Galileo mezo ebben a valaszban - a SIM7000 firmware
// ezt nem adja vissza AT+CGNSSINFO-val, csak nyers NMEA adatbol lenne
// kinyerheto (amit ez a kod nem olvas), ezert a Galileo mezo mindig "n/a" marad.
void gnssPollExtendedSats() {
  modem.sendAT("+CGNSSINFO");
  if(modem.waitResponse(700L, GF("+CGNSSINFO:")) != 1){
    modem.waitResponse(100L);
    yield();
    return;
  }
  yield();
  String raw = modemSerial.readStringUntil('\n');
  raw.trim();
  gGnss.rawCgnssinfo = raw;
  modem.waitResponse(100L);
  yield();

  String g[16];
  int gi = 0, pv = 0;
  for(int i=0; i<=(int)raw.length() && gi<16; i++){
    if(i==(int)raw.length() || raw[i]==','){
      g[gi++] = raw.substring(pv, i);
      pv = i+1;
    }
  }
  // Legalabb a mode+GPS+GLONASS+BEIDOU mezoknek meg kell lenniuk
  if(gi >= 4) {
    gGnss.satGPS = g[1].length() ? g[1].toInt() : -1;
    gGnss.satGLO = g[2].length() ? g[2].toInt() : -1;
    gGnss.satBDS = g[3].length() ? g[3].toInt() : -1;
    gGnss.satGAL = -1; // ez a parancs nem adja vissza, marad n/a
    gGnss.satTotalView = (gGnss.satGPS>=0?gGnss.satGPS:0)
                        + (gGnss.satGLO>=0?gGnss.satGLO:0)
                        + (gGnss.satBDS>=0?gGnss.satBDS:0);
  } else {
    gGnss.lastError = "Hianyos CGNSSINFO valasz (" + String(gi) + " mezo): " + raw;
  }
}

void gnssPollAntenna() {
  modem.sendAT("+CGNSANT");
  if(modem.waitResponse(500L, GF("+CGNSANT:")) != 1){
    modem.waitResponse(100L);
    yield();
    gGnss.antStatus = -1;
    return;
  }
  yield();
  String raw = modemSerial.readStringUntil('\n');
  raw.trim();
  modem.waitResponse(100L);
  yield();
  gGnss.antStatus = raw.toInt();
}

unsigned long gLastGnssPoll = 0;
uint8_t gGnssPollStep = 0;
void gnssLoop() {
  if(!gGnss.enabled) return;
  if(gModem.callActive) return;
  if(millis() - gLastGnssPoll < GNSS_POLL_INTERVAL_MS) return;
  gLastGnssPoll = millis();
  gGnss.lastPoll = millis();

  if(gGnssPollStep == 0) {
    gnssPollPosition();
    gGnss.lastPositionPoll = millis();
  } else if(gGnssPollStep == 1) {
    gnssPollExtendedSats();
    gGnss.lastExtendedPoll = millis();
  } else {
    gnssPollAntenna();
    gGnss.lastAntennaPoll = millis();
  }
  gGnssPollStep = (gGnssPollStep + 1) % 3;
}