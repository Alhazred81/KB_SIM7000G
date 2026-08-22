//gnss_mgr.h

#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "time_mgr.h"
#include "modem_types.h"
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
void gnssLoadAssist();
void gnssSaveAssist(float lat, float lon);
String gnssReceiverStatusText();
String gnssCompassDir(float course);
void gnssStart();
void gnssStop();
void gnssPollPosition();
void gnssPollExtendedSats();
void gnssPollAntenna();
void gnssLoop();
extern uint8_t gPosReportDays;
void gnssSaveConfig(uint8_t days);
