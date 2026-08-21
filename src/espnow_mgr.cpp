#include "espnow_mgr.h"

HiveRecord gHiveRecords[MAX_ESP_NOW_HIVES];
int gHiveCount = 0;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_data)) {
    Serial.printf("[ESP-NOW] Hibas csomagmeret! Vart: %d, Kapott: %d\n", sizeof(struct_data), len);
    return;
  }

  struct_data incoming;
  memcpy(&incoming, incomingData, sizeof(incoming));

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  Serial.printf("[ESP-NOW] Csomag erkezett: %s, Belso hom: %.1f C\n", macStr, incoming.tempInt);

  bool found = false;
  for (int i = 0; i < gHiveCount; i++) {
    if (gHiveRecords[i].macAddress == String(macStr)) {
      gHiveRecords[i].data = incoming;
      gHiveRecords[i].lastSeen = millis();
      found = true;
      break;
    }
  }

  if (!found && gHiveCount < MAX_ESP_NOW_HIVES) {
    gHiveRecords[gHiveCount].macAddress = String(macStr);
    gHiveRecords[gHiveCount].data = incoming;
    gHiveRecords[gHiveCount].lastSeen = millis();
    gHiveCount++;
  }
}

void initEspNowGateway(uint8_t channel) {
  esp_now_deinit();
  if (esp_now_init() != ESP_OK) {
    Serial.println(F("[ESP-NOW] Inicializalasi hiba."));
    return;
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  Serial.printf("[ESP-NOW] Gateway elindult a %d. csatornan.\n", channel);
}