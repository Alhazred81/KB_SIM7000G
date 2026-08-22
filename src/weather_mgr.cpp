//weather_mgr.cpp

#include "weather_mgr.h"
#include <ArduinoJson.h>

#ifndef TINY_GSM_RX_BUFFER
#define TINY_GSM_RX_BUFFER 1024
#endif
#define TINY_GSM_MODEM_SIM7000
#include <TinyGsmClient.h>

#include "modem_mgr.h"
#include "gnss_mgr.h"
#include "time_mgr.h"

extern TinyGsm modem; 
extern GnssState gGnss;
extern TimeState gTime;

// Ha az ntfy küldő függvényed máshol van deklarálva, illeszd be az extern-t,
// vagy hívd a saját ntfy triggeredet:
// extern void sendNtfyAlert(String title, String message, int priority);

DailyForecast gForecast[3];
unsigned long gLastWeatherSync = 0;
bool gWeatherHasData = false;

void weatherInit() {
  gWeatherHasData = false;
}

bool weatherUpdate(float lat, float lon) {
  Serial.printf("[WEATHER] Idojaras-szinkronizacio inditasa (Lat: %.4f, Lon: %.4f)...\n", lat, lon);

  char resource[250];
  // HOZZÁADVA: hourly paraméterekhez a weathercode is!
  snprintf(resource, sizeof(resource), 
    "/v1/forecast?latitude=%.4f&longitude=%.4f&hourly=temperature_2m,precipitation,weathercode&timezone=Europe/Budapest&forecast_days=3", 
    lat, lon);

  const char* server = "api.open-meteo.com";
  const int port = 80;

  TinyGsmClient client(modem, 0);

  Serial.println(F("[WEATHER] Kapcsolodas az Open-Meteo szerverhez..."));
  if (!client.connect(server, port)) {
    Serial.println(F("[WEATHER] HIBA: Nem sikerult kapcsolodni az API-hoz!"));
    return false;
  }

  client.print(String("GET ") + resource + " HTTP/1.1\r\n");
  client.print(String("Host: ") + server + "\r\n");
  client.print("Connection: close\r\n\r\n");

  // 1. Fejlécek átlépése
  uint32_t timeout = millis();
  bool headersEnded = false;
  
  while (client.connected() && millis() - timeout < 10000L) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) {
        headersEnded = true;
        break;
      }
    }
    if (headersEnded) break;
  }

  if (!headersEnded) {
    Serial.println(F("[WEATHER] HIBA: Idotullepes vagy hianyos HTTP fejlec!"));
    client.stop();
    return false;
  }

  // 2. Chunkolt adatok olvasása
  String rawJson = "";
  while (client.connected() || client.available()) {
    String chunkSizeLine = client.readStringUntil('\n');
    chunkSizeLine.trim();
    if (chunkSizeLine.length() == 0) continue;

    int chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);
    if (chunkSize == 0) break; 

    char* chunkBuffer = new char[chunkSize + 1];
    int bytesRead = client.readBytes(chunkBuffer, chunkSize);
    chunkBuffer[bytesRead] = '\0';
    
    rawJson += String(chunkBuffer);
    delete[] chunkBuffer;

    client.readStringUntil('\n');
  }

  client.stop();

  if (rawJson.length() == 0) {
    Serial.println(F("[WEATHER] HIBA: Ures valasz erkezett a chunkokbol!"));
    return false;
  }

  // 3. JSON feldolgozása
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, rawJson);

  if (error) {
    Serial.printf("[WEATHER] HIBA: JSON feldolgozas sikertelen: %s\n", error.c_str());
    return false;
  }

  for (int i = 0; i < 3; i++) {
    for (int b = 0; b < 4; b++) {
      gForecast[i].blocks[b].tempMin = 99.0;
      gForecast[i].blocks[b].tempMax = -99.0;
      gForecast[i].blocks[b].precip = 0.0;
      gForecast[i].blocks[b].weatherCode = 0;
    }
  }

  JsonArray timeArr = doc["hourly"]["time"];
  JsonArray tempArr = doc["hourly"]["temperature_2m"];
  JsonArray precipArr = doc["hourly"]["precipitation"];
  JsonArray codeArr = doc["hourly"]["weathercode"]; // ÚJ: Időjárás kódok tömbje

  if (timeArr.isNull() || tempArr.isNull() || codeArr.isNull()) {
    Serial.println(F("[WEATHER] HIBA: A JSON nem tartalmaz 'hourly' adatokat."));
    return false;
  }

  bool severeWeatherDetected = false;

  for (int i = 0; i < 72; i++) {
    int day = i / 24;
    int block = (i % 24) / 6;
    
    if (i % 24 == 0) {
      String fullTime = timeArr[i].as<String>();
      gForecast[day].dateStr = fullTime.substring(5, 10); 
    }

    float temp = tempArr[i].as<float>();
    float precip = precipArr[i].as<float>();
    int code = codeArr[i].as<int>();

    if (temp < gForecast[day].blocks[block].tempMin) gForecast[day].blocks[block].tempMin = temp;
    if (temp > gForecast[day].blocks[block].tempMax) gForecast[day].blocks[block].tempMax = temp;
    gForecast[day].blocks[block].precip += precip;
    
    // Elmentjük a legsúlyosabb időjárás kódot erre a blokkra
    if (code > gForecast[day].blocks[block].weatherCode) {
      gForecast[day].blocks[block].weatherCode = code;
    }

    // Vihar (95) vagy Jégeső (96, 99) detektálása az előrejelzésben
    if (code == 96 || code == 99 || code == 95 || precip > 6.0) {
      severeWeatherDetected = true;
    }
  }

  // Ha veszélyes időt (vihar / jég) hoznak a modellek, küldünk egy maximális prioritású riasztást
  if (severeWeatherDetected) {
    Serial.println(F("[WEATHER] 🧊⚡ FIGYELEM: Extrém időjárás (Vihar / Jég) várható a következő napokban!"));
    // Itt triggerelheted az NTFY riasztást (5-ös prió):
    // sendNtfyAlert("VIGYÁZAT: Vihar vagy Jégeső!", "Az elorejelzes alapjan veszelyes idojaras (vihar/jeg) kozeleg!", 5);
  }

  gWeatherHasData = true;
  gLastWeatherSync = millis();
  
  Serial.println(F("[WEATHER] SIKER: 3 napos elorejelzes frissitve (időjárás kódokkal)!"));
  return true;
}

void backgroundTaskLoop() {
  static unsigned long lastCheckTime = 0;
  if (millis() - lastCheckTime < 30000UL) return;
  lastCheckTime = millis();

  if (!gTime.synced) return;

  unsigned long nowMillis = millis();
  unsigned long twelveHours = 12UL * 3600UL * 1000UL;

  if (gLastWeatherSync == 0 || (nowMillis - gLastWeatherSync > twelveHours)) {
    float activeLat = gGnss.fix ? gGnss.lat : gGnss.assistLat;
    float activeLon = gGnss.fix ? gGnss.lon : gGnss.assistLon;

    if (activeLat != 0.0 && activeLon != 0.0) {
      Serial.println(F("[SYSTEM] Utemezett idojaras-szinkronizacio inditasa a hatterben..."));
      weatherUpdate(activeLat, activeLon);
    }
  }
}