#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

typedef struct __attribute__((packed)) AudioBandStats {
  uint8_t min_val, max_val, avg_val, median_val;
} AudioBandStats;

typedef struct __attribute__((packed)) struct_data {
  uint8_t msgType;
  float tempExt, tempInt, humidity;
  int32_t weight_raw;      
  AudioBandStats audioBands[6]; 
  uint32_t checksum;
} struct_data;

#define MAX_ESP_NOW_HIVES 20

struct HiveRecord {
  String macAddress;
  struct_data data;
  unsigned long lastSeen;
};

extern HiveRecord gHiveRecords[MAX_ESP_NOW_HIVES];
extern int gHiveCount;

void initEspNowGateway(uint8_t channel);