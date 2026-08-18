#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "crypto.h"
#include "time_mgr.h" // ntpStart miatt

// ─── Hálózati üzemmód ────────────────────────────────────────
enum class NetMode { AP, STA_CONNECTING, STA_CONNECTED, STA_FAILED };

struct WifiStaState {
  NetMode mode = NetMode::AP;
  String  targetSSID  = "";
  String  targetPass  = "";
  unsigned long connectStarted = 0;
  unsigned long lastAttempt    = 0;
  String  lastError = "";
  String  ip = "";
};

extern WifiStaState gSta;

// Előre deklarált függvény a main.cpp-ből (AP indítás)
void startAP();

// ─── Elérhető hálózatok szkennelése ─────────────────────────
struct ScannedNet {
  String ssid;
  int rssi;
  bool secure;
};

#define MAX_SCAN_RESULTS 15
extern ScannedNet gScanResults[MAX_SCAN_RESULTS];
extern int gScanCount;
extern unsigned long gLastScan;

void wifiScan();

// ─── Csatlakozás indítása egy adott hálózatra ────────────────
void wifiStaConnect(const String& ssid, const String& pass);

// ─── STA állapot loop ────────────────────────────────────────
void wifiStaLoop();

// ─── Indulási auto-csatlakozás ───────────────────────────────
void wifiStaTryAutoConnect();

// ─── STA lecsatlakozás ───────────────────────────────────────
void wifiStaDisconnect();

// ─── Kapcsolat-vesztés figyelése ─────────────────────────────
extern unsigned long gLastStaCheck;
void wifiStaWatchdog();