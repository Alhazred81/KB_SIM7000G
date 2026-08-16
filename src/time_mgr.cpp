//time_mgr.cpp

#include "time_mgr.h"

TimeState gTime;

String formatLocalTime() {
  struct tm tmInfo;
  if(!getLocalTime(&tmInfo, 20)) return "-";

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmInfo);

  return String(buf);
}

void ntpStart() {
  if(WiFi.status() != WL_CONNECTED) return;

  configTzTime(
    "CET-1CEST,M3.5.0/2,M10.5.0/3",
    "pool.ntp.org",
    "time.nist.gov",
    "time.google.com"
  );

  gTime.started = true;
  gTime.synced = false;
  gTime.lastCheck = 0;

  Serial.println(F("[NTP] Ido szinkron inditva (Europe/Budapest)."));
}

void ntpLoop() {
  if(!gTime.started || WiFi.status() != WL_CONNECTED) return;
  if(millis() - gTime.lastCheck < 10000UL) return;

  gTime.lastCheck = millis();

  String now = formatLocalTime();

  if(now != "-") {
    bool firstSync = !gTime.synced;

    gTime.synced = true;
    gTime.localTime = now;

    if(firstSync) {
      Serial.println("[NTP] Ido szinkron OK: " + gTime.localTime);
      modemSetTimeFromSystem();
    }
  }
}

bool modemSetTimeFromSystem() {
return false;
}