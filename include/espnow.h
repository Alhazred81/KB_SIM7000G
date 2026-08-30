#ifndef ESPNOW_H
#define ESPNOW_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// A kaptármonitor által küldött csomag felépítése
typedef struct struct_message {
  uint8_t magic;         // Biztonsági bájt (pl. 0xBE)
  float weight;          // Kompenzált súly (kg)
  float tempInt;         // Belső hőmérséklet a fészeknél (°C)
  float humidity;        // Belső páratartalom (%)
  float vbat;            // Akku feszültség (V)
  uint8_t flags;         // Állapot bitek
  uint32_t activeTimeMs; // ÚJ: Mennyi ideig volt ébren küldés előtt (millis)
} struct_message;

bool initWiFiAndEspNow(wifi_mode_t mode, String ssid, String pass, uint8_t channel, bool hideSSID);

// Webes teszteléshez szükséges logoló függvények (csak a szerveren kell használni)
String getEspNowLog();
void clearEspNowLog();

#endif // ESPNOW_H