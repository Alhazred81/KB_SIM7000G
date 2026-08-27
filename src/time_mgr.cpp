// time_mgr.cpp
#include <Arduino.h>
#include <sys/time.h>
#include <time.h>
#include "time_mgr.h"
#include "modem_mgr.h" // A gModem.ready eléréséhez

extern ModemState gModem;

TimeState gTime;

String formatLocalTime() {
  struct tm tmInfo;
  // Kiolvassuk az ESP32 belső RTC-jét
  if(!getLocalTime(&tmInfo, 20)) return "-";

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmInfo);
  return String(buf);
}

void ntpStart() {
  // A függvény neve szándékosan maradt ntpStart, hogy a main.cpp-t ne kelljen átírni!
  Serial.println(F("[TIME] Várakozás a modem hálózati idejére..."));
  gTime.started = true;
  gTime.synced = false;
  gTime.lastCheck = 0;
}

void ntpLoop() {
  if(!gTime.started) return;

  // Ha már megtörtént a szinkron, másodpercenként csak a szöveges változót frissítjük az RTC-ből
  if (gTime.synced) {
    if(millis() - gTime.lastCheck > 1000UL) {
      gTime.lastCheck = millis();
      String now = formatLocalTime();
      if(now != "-") gTime.localTime = now;
    }
    return;
  }

  // Amíg a modem nincs a hálózaton (READY), nem bombázzuk az AT paranccsal
  if (!gModem.ready) return;

  // Szinkronizáció előtt 10 másodpercenként próbálkozunk
  if(millis() - gTime.lastCheck < 10000UL) return;
  gTime.lastCheck = millis();

  // Lekérjük a mobilhálózat idejét
  String resp = modemAtQuery("AT+CCLK?", 1500);
  
  // Parse: +CCLK: "26/08/27,07:43:54+08"
  int startIdx = resp.indexOf("\"");
  int endIdx = resp.lastIndexOf("\"");

  if (startIdx != -1 && endIdx != -1 && (endIdx - startIdx >= 17)) {
    String cclk = resp.substring(startIdx + 1, endIdx);

    struct tm t;
    memset(&t, 0, sizeof(struct tm));

    t.tm_year = (cclk.substring(0, 2).toInt() + 2000) - 1900;
    t.tm_mon  = cclk.substring(3, 5).toInt() - 1;
    t.tm_mday = cclk.substring(6, 8).toInt();
    t.tm_hour = cclk.substring(9, 11).toInt();
    t.tm_min  = cclk.substring(12, 14).toInt();
    t.tm_sec  = cclk.substring(15, 17).toInt();
    t.tm_isdst = -1;

    time_t epochTime = mktime(&t);
    if (epochTime >= 0) {
      // Beállítjuk az ESP32 belső óráját (RTC) a modemtől kapott adatokkal
      struct timeval tv = { .tv_sec = epochTime, .tv_usec = 0 };
      settimeofday(&tv, NULL);

      gTime.synced = true;
      gTime.localTime = formatLocalTime();
      Serial.println("[TIME] Időszinkron OK (Modemről): " + gTime.localTime);
    }
  }
}