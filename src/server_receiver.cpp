#include "server_receiver.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

unsigned long currentServerTimestamp = 1787680000; 

void sendAckToMonitor(const uint8_t *mac_addr) {
  JsonDocument ackDoc;
  ackDoc["comm_type"] = "comm_ack";
  ackDoc["sleep_min"] = 15;
  ackDoc["timestamp"] = currentServerTimestamp;

  String ackPayload;
  serializeJson(ackDoc, ackPayload);

  if (!esp_now_is_peer_exist(mac_addr)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac_addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  esp_err_t result = esp_now_send(mac_addr, (uint8_t *)ackPayload.c_str(), ackPayload.length());
  if (result == ESP_OK) {
    Serial.println("[ESP-NOW] Nyugta (ack_cmd) sikeresen elküldve.");
  } else {
    Serial.println("[ESP-NOW] Hiba a nyugta küldésekor!");
  }
}

// Itt a változás: a mac_addr közvetlenül jön, nem szerkezetből
void onEspNowReceive(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  String payload = String((char*)incomingData, len);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("[ESP-NOW] JSON olvasási hiba: ");
    Serial.println(error.c_str());
    return;
  }

  String commType = doc["comm_type"] | "unknown";
  String deviceMac = doc["device_mac"] | "UNKNOWN_MAC";

  if (commType != "comm_tel" && commType != "comm_aud") {
    Serial.println("[ESP-NOW] Ismeretlen comm_type érkezett.");
    return;
  }

  String cleanMac = deviceMac;
  cleanMac.replace(":", "");
  
  String filename = "/" + commType + "_" + cleanMac + "_" + String(millis()) + ".json";

  File file = LittleFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[FS Hiba] Nem sikerült megnyitni a fájlt írásra a LittleFS-en!");
    return;
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println("[FS Hiba] Nem sikerült a JSON-t a fájlba írni!");
  } else {
    Serial.printf("[FS] Adat mentve: %s (Méret: %d byte)\n", filename.c_str(), file.size());
  }
  file.close();

  sendAckToMonitor(mac_addr);
}

void initServerEspNow() {
  if(!LittleFS.begin(true)){
    Serial.println("[FS Hiba] Nem sikerült inicializálni a LittleFS-t!");
    return;
  }
  Serial.println("[FS] LittleFS sikeresen indulva.");

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Hiba az ESP-NOW indításakor!");
    return;
  }

  esp_now_register_recv_cb(onEspNowReceive);
  Serial.println("[ESP-NOW] Szerver készen áll, hallgatózik...");
}