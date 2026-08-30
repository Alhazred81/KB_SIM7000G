//gnss_mgr.cpp

#include "gnss_mgr.h"
#include "time_mgr.h"
#include "modem_mgr.h"

// 1. A VÁLTOZÓ TÉNYLEGES LÉTREHOZÁSA (Nincs extern!)
PreciseSurvey gSurvey;

unsigned long gLastGnssPoll = 0;
uint8_t gGnssPollStep = 0;
uint8_t gPosReportDays = 0;

static float lastLoggedLat = 0.0;
static float lastLoggedLon = 0.0;

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

void gnssSaveAssist(float lat, float lon, float hdop) {
  // --- ÚJ LAKAT ---
  static bool alreadySavedThisBoot = false;
  if (alreadySavedThisBoot) {
    return; // Ebben a bekapcsolási ciklusban már mentettünk, nem írjuk feleslegesen a memóriát!
  }
  alreadySavedThisBoot = true;
  // ---------------

  // gGnss.assistSaved = true;  <-- EZT A SORT TÖRÖLTÜK!
  gGnss.assistLat = lat;
  gGnss.assistLon = lon;

  EEPROM.write(ADDR_GNSS_ASSIST_FLAG, MAGIC_BYTE);
  EEPROM.put(ADDR_GNSS_ASSIST_LAT, lat);
  EEPROM.put(ADDR_GNSS_ASSIST_LON, lon);
  EEPROM.commit();

  Serial.printf("[GNSS] Kiindulo koordinata mentve: %.6f, %.6f (HDOP: %.1f)\n", lat, lon, hdop);
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
  modem.sendAT("+SGPIO=0,4,1,1"); 
  modem.waitResponse(1000L);
  
  modem.sendAT("+CGNSPWR=0"); modem.waitResponse(1000L); yield();
  delay(300); yield();
  modem.sendAT("+CGNSPWR=1"); modem.waitResponse(2000L); yield();
  delay(500); yield();
  
  modem.sendAT("+CGNSMOD=1,1,1,1");
  modem.waitResponse(1000L);

  if (gTime.synced) {
    // NTP szinkron logika helye
  }
  
  if (!isnan(gGnss.assistLat) && !isnan(gGnss.assistLon)) {
    String assistCmd = "AT+CGNSGPS=1," + String(gGnss.assistLat, 6) + "," + String(gGnss.assistLon, 6) + ",0";
    modem.sendAT(assistCmd);
    modem.waitResponse(2000L);
  }

  gGnss.enabled = true;
  gGnss.startedAt = millis();
  gGnss.lastError = "";
  Serial.println(F("[GNSS] Inditva (GPS+GLONASS+BDS+GALILEO) + Antenna táp bekapcsolva."));
  Serial.println("[GNSS] Kiindulo koordinata: " + String(gGnss.assistLat, 6) + ", " + String(gGnss.assistLon, 6));
}

void gnssStop() {
  modem.sendAT("+CGNSPWR=0");
  modem.waitResponse(1000L);
  gGnss.enabled = false;
  gGnss.fix = false;
  Serial.println(F("[GNSS] Leallitva."));
}

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

  gGnss.satGpsInView = f[14].length() ? f[14].toInt() : -1;

  if(gGnss.fix){
    gGnss.lat    = f[3].toFloat();
    gGnss.lon    = f[4].toFloat();
    gGnss.alt    = f[5].toFloat();
    gGnss.speed  = f[6].toFloat();
    gGnss.course = f[7].toFloat();
    gGnss.hdop   = f[10].toFloat();
    gGnss.satUsed = f[15].length() ? f[15].toInt() : 0;
    gGnss.lastGoodFix = millis();

    gnssSaveAssist(gGnss.lat, gGnss.lon, gGnss.hdop);

    String utc = f[2];
    if(utc.length() >= 14){
      gGnss.dateStr = utc.substring(6,8) + "." + utc.substring(4,6) + "." + utc.substring(0,4);
      gGnss.timeStr = utc.substring(8,10) + ":" + utc.substring(10,12) + ":" + utc.substring(12,14);
    }
  } else {
    gGnss.satUsed = 0;
  }
}

void updateGnssAssist(float lat, float lon, float hdop) {
    bool isStable = (hdop < 2.0 && abs(lat - lastLoggedLat) < 0.00001 && abs(lon - lastLoggedLon) < 0.00001);
    
    if (!isStable) {
        Serial.printf("[GNSS] Kiindulo koordinata mentve: %.6f, %.6f\n", lat, lon);
        lastLoggedLat = lat;
        lastLoggedLon = lon;
    }
}

void logGnssPosition(float lat, float lon, float hdop) {
    float deltaLat = abs(lat - lastLoggedLat);
    float deltaLon = abs(lon - lastLoggedLon);
    
    if (hdop < 2.5 && deltaLat < 0.00002 && deltaLon < 0.00002) {
        return; 
    }

    Serial.printf("[GNSS] Kiindulo koordinata mentve: %.6f, %.6f (HDOP: %.1f)\n", lat, lon, hdop);
    lastLoggedLat = lat;
    lastLoggedLon = lon;
}

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
  if(gi >= 4) {
    gGnss.satGPS = g[1].length() ? g[1].toInt() : -1;
    gGnss.satGLO = g[2].length() ? g[2].toInt() : -1;
    gGnss.satBDS = g[3].length() ? g[3].toInt() : -1;
    gGnss.satGAL = -1; 
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

void gnssLoop() {
  if(!gGnss.enabled) return;
  if(gModem.callActive) return;
  
  // 2. PRECÍZIÓS BEMÉRÉS FUTTATÁSA
  preciseSurveyLoop();

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

void gnssSaveConfig(uint8_t days) {
  gPosReportDays = days;
  EEPROM.write(ADDR_POS_DAYS, days);
  EEPROM.commit();
}

// 3. JAVÍTOTT AT PARANCS A TINYGSM SZINTAXISÁVAL
void startPreciseSurvey() {
  gSurvey.active = true;
  gSurvey.startTime = millis();
  gSurvey.sampleCount = 0;
  gSurvey.latSum = 0.0;
  gSurvey.lonSum = 0.0;
  gSurvey.bestHdop = 99.9;
  gSurvey.finalLat = 0.0;
  gSurvey.finalLon = 0.0;
  
  modem.sendAT("+CGNSPWR=1"); 
  modem.waitResponse(1000L);
  Serial.println("[GNSS] Precíziós kaptár-bemérés elindítva (30s átlagolás)...");
}

void preciseSurveyLoop() {
  if (!gSurvey.active) return;

  unsigned long elapsed = millis() - gSurvey.startTime;
  
  if (elapsed > gSurvey.durationMs) {
    gSurvey.active = false;
    if (gSurvey.sampleCount > 0) {
      gSurvey.finalLat = gSurvey.latSum / gSurvey.sampleCount;
      gSurvey.finalLon = gSurvey.lonSum / gSurvey.sampleCount;
      Serial.printf("[GNSS] Bemérés kész! Pozíció: %.6f, %.6f (Minták: %d, HDOP: %.1f)\n", 
          gSurvey.finalLat, gSurvey.finalLon, gSurvey.sampleCount, gSurvey.bestHdop);
    } else {
      Serial.println("[GNSS] Bemérés sikertelen: nem érkezett érvényes fix.");
    }
    return;
  }

  if (gGnss.fix && gGnss.lat != 0.0 && gGnss.lat != gSurvey.finalLat) {
    static double lastAddedLat = 0.0;
    if (gGnss.lat != lastAddedLat) {
      gSurvey.latSum += gGnss.lat;
      gSurvey.lonSum += gGnss.lon;
      gSurvey.sampleCount++;
      lastAddedLat = gGnss.lat;
      
      if (gGnss.hdop < gSurvey.bestHdop) {
        gSurvey.bestHdop = gGnss.hdop;
      }
    }
  }
}