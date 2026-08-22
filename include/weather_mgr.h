#pragma once
#include <Arduino.h>

struct ForecastBlock {
  float tempMin = 99.0;
  float tempMax = -99.0;
  float precip = 0.0;
  int weatherCode = 0;
};

struct DailyForecast {
  String dateStr;
  ForecastBlock blocks[4]; // 0: 00-06, 1: 06-12, 2: 12-18, 3: 18-00
};

extern DailyForecast gForecast[3];
extern unsigned long gLastWeatherSync;
extern bool gWeatherHasData;

void weatherInit();
bool weatherUpdate(float lat, float lon);