#ifndef SERVER_RECEIVER_H
#define SERVER_RECEIVER_H

#include <Arduino.h>
#include <esp_now.h>

extern unsigned long currentServerTimestamp;

void initServerEspNow();
void onEspNowReceive(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void sendAckToMonitor(const uint8_t *mac_addr);

#endif // SERVER_RECEIVER_H