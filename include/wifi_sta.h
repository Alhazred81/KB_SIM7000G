#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "crypto.h"
#include "time_mgr.h"

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

void startAP();
String loadApSSID();
void saveApSSID(const String& ssid);

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
void wifiStaConnect(const String& ssid, const String& pass);
void wifiStaLoop();
void wifiStaTryAutoConnect();
void wifiStaDisconnect();
void wifiStaWatchdog();

void saveApConfig(const String& ssid, const String& pass, uint8_t channel, bool hide);
bool loadApHide();