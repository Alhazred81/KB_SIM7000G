//web_diag.h

#ifndef WEB_DIAG_H
#define WEB_DIAG_H

#include <Arduino.h>

void handleDiag();
void handleAtAjax();
void handleAtStatus();
void handleModemStatus();
void handleReinit();
void handleExpert();
void handleExpertPost();
void handleExpertReset();
void handleExpertFullReset();
void handleEspRestart();

#endif