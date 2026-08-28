#ifndef WEB_DIAG_H
#define WEB_DIAG_H

void handleEspRestart();
void handleExpert();
void handleExpertPost();
void handleExpertReset();
void handleExpertFullReset();
void handleDiag();
void handleAtAjax();
void handleAtStatus();
void handleModemStatus();
void handleReinit();

void handleGetHivesJson();
void handleDeleteHive();
void handleAddDummyHive();
void handleAtStatusSerial(); 

#endif