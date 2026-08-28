//gnss_mgr.h

#ifndef GNSS_MGR_H
#define GNSS_MGR_H

#include <Arduino.h>
#include <EEPROM.h>

// Alapvető GNSS adatszerkezet (a korábbi kódjaid alapján)
struct GnssState {
  bool enabled = false;
  bool fix = false;
  int runStatus = 0;
  int fixStatus = 0;
  float lat = 0.0;
  float lon = 0.0;
  float alt = 0.0;
  float speed = 0.0;
  float course = 0.0;
  float hdop = 99.9;
  int satGpsInView = 0;
  int satUsed = 0;
  int satGPS = 0;
  int satGLO = 0;
  int satBDS = 0;
  int satGAL = 0;
  int satTotalView = 0;
  int antStatus = -1;
  String dateStr = "";
  String timeStr = "";
  String rawCgnsinf = "";
  String rawCgnssinfo = "";
  String lastError = "";
  
  float assistLat = 0.0;
  float assistLon = 0.0;
  
  unsigned long startedAt = 0;
  unsigned long lastGoodFix = 0;
  unsigned long lastPoll = 0;
  unsigned long lastPositionPoll = 0;
  unsigned long lastExtendedPoll = 0;
  unsigned long lastAntennaPoll = 0;
};

extern GnssState gGnss;
extern unsigned long gLastGnssPoll;
extern uint8_t gPosReportDays;

void gnssLoadAssist();
void gnssSaveAssist(float lat, float lon, float hdop);
String gnssReceiverStatusText();
String gnssCompassDir(float course);
void gnssStart();
void gnssStop();
void gnssPollPosition();
void gnssPollExtendedSats();
void gnssPollAntenna();
void gnssLoop();
void gnssSaveConfig(uint8_t days);
void updateGnssAssist(float lat, float lon, float hdop);
void logGnssPosition(float lat, float lon, float hdop);

// --- ÚJ: KAPTÁR BEMÉRÉS STRUKTÚRÁK ---
struct PreciseSurvey {
  bool active = false;
  unsigned long startTime = 0;
  unsigned long durationMs = 30000UL;
  int sampleCount = 0;
  double latSum = 0.0;
  double lonSum = 0.0;
  float bestHdop = 99.9;
  double finalLat = 0.0;
  double finalLon = 0.0;
};

extern PreciseSurvey gSurvey;

void startPreciseSurvey();
void preciseSurveyLoop();

#endif // GNSS_MGR_H