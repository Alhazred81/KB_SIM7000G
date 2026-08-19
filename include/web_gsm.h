//web_gsm.h

#ifndef WEB_GSM_H
#define WEB_GSM_H

#include <Arduino.h>

void handleGsm();
void handleDoSms();
void handleSmsStatus();
void handleDoCall();
void handleHangup();
void handleSetSmsc();
void handleNetAuto();
void handleNetScan();
void handleNetManual();

#endif