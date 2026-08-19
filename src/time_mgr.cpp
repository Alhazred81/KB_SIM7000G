//time_mgr.cpp

#include <Arduino.h>
#include <WiFi.h>
#include "time_mgr.h"

TimeState gTime;
extern HardwareSerial modemSerial; // <-- Ide beillesztve

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
// time_mgr.cpp-hez tartozó kiegészítés

void syncModemClockWithNtp() {
  if (!gTime.synced) return;
  String t = gTime.localTime; 
  if (t.length() >= 19) {
    String yy = t.substring(2, 4);
    String mo = t.substring(5, 7);
    String dd = t.substring(8, 10);
    String hh = t.substring(11, 13);
    String mi = t.substring(14, 16);
    String ss = t.substring(17, 19);
    
    // Mivel CET/CEST zónában vagyunk (+2 óra nyáron, ami 8 negyedóra -> +08)
    String cclk = "AT+CCLK=\"" + yy + "/" + mo + "/" + dd + "," + hh + ":" + mi + ":" + ss + "+08\"";
    
    // Használjuk a közvetlen soros parancsot, ami a modem_mgr-en keresztül mindenhol elérhető:
    modemSerial.println(cclk);
    // Beolvassuk a választ, hogy kiürüljön a puffer
    unsigned long start = millis();
    while (millis() - start < 1000) {
      if (modemSerial.available()) {
        modemSerial.readStringUntil('\n');
      }
      yield();
    }
  }
}

bool modemSetTimeFromSystem() {
  syncModemClockWithNtp();
  return true;
}