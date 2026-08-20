//web_config.h

#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>

void handleCfg();
void handleWifiScan();
void handleStaConnect();
void handleStaDisconnect();
void handleSaveWifi();
void handleTestSavePin();
void handleConfirmSavePin();
void handleSavePin();
void handleChangePin();
void handleSavePanelVer();
void handleLedTrigger();
void handleLedAuto();

#endif