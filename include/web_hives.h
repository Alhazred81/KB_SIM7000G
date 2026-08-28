 // WEB_HIVES_H

#ifndef WEB_HIVES_H
#define WEB_HIVES_H

#include <Arduino.h>

// --- Korábbi kaptárkezelő funkciók ---
void handleHives();
void handleHiveView();
void handleEvaluation();
void handleTreatment();
void handleGetTreatmentsJson();
void handleEvaluatePost();
void handleGetEvaluationsJson();
void handleGetColonyFunctionsJson();
void handleGetDiseasesJson();

// --- KAPTÁR REGISZTRÁCIÓS VARÁZSLÓ ---
struct HiveRegistrationContext {
  bool active = false;
  String hiveId = "";
  int totalBoxes = 0;
  String nfcUids[10];
  String queenOrigin = "";
  int queenVintage = 0;
  double finalLat = 0.0;
  double finalLon = 0.0;
};

extern HiveRegistrationContext gRegCtx;

void handleRegStart();
void handleRegNfc();
void handleRegQueen();
void handleRegSurvey();
void handleApiSurveyStatus();
void handleRegSummary();
void handleRegSave();
void handleRegCancel();

#endif