// modem_types.h
#pragma once

#include <Arduino.h>

// ─── Állapot struktúra ───────────────────────────────────────
struct ModemState {
  bool    powered     = false;
  bool    ready       = false;
  bool    registered  = false;
  bool    pinOk       = false;
  bool    callActive  = false;
  int     ringCount   = 0;
  unsigned long callStart = 0;
  String  simCCID     = "";
  String  simIMEI     = "";
  String  operatorName= "";
  int     signalQuality = 0;  // 0-31, 99=unknown
  String  netType     = "";
  String  lastError   = "";
  // Diagnosztika
  unsigned long bootTime = 0;
  int     initAttempts   = 0;
  bool    uartResponding = false; // a modem valaszolt-e egyaltalan AT parancsra
  // Init-folyamat élő státusza (webUI-n kirajzolható, restart alatt is)
  bool    initInProgress = false;
  String  initPhase      = "";   // aktuális lépés szövegesen
  int     initPhaseNum   = 0;    // 0-5, a lépés sorszáma
  static const int INIT_PHASE_MAX = 5;
  // SMS diagnosztika
  String  smscNumber     = "";   // AT+CSCA? aktuális eredménye, "" = még nem kérdeztük le
  bool    smscChecked    = false;
  unsigned long smscCheckedAt = 0;
};