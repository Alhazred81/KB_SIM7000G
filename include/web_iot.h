//web_iot.h 

#ifndef WEB_IOT_H
#define WEB_IOT_H

#include <Arduino.h>

void handleIot();
void handleDataOn();
void handleDataOff();
void handleDataPing();

void handleNtfySend();
void handleNtfyPoll();
void handleSaveNtfy();

void handleSaveReport();
void handleTestReport();
void checkAndSendScheduledReport();

void saveReportConfig(const String& times);
String loadReportConfig();

#endif