//time_mgr.h

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

bool modemSetTimeFromSystem();

struct TimeState {
  bool started = false;
  bool synced = false;
  unsigned long lastCheck = 0;
  String localTime = "-";
};

extern TimeState gTime;

String formatLocalTime();
void ntpStart();
void ntpLoop();