//modem_mgr.h

#pragma once
#include <Arduino.h>
#include "config.h"
#include "crypto.h"
#include "modem_utils.h"
#include "modem_types.h"
#include "NtfyClient.h" 
#include "time_mgr.h"

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024

#include <TinyGsmClient.h>

extern HardwareSerial modemSerial;
extern TinyGsm modem;

extern bool gSmsSendRequested;
extern bool gSmsSendInProgress;
extern bool gModemInitRequested;

extern ModemState gModem;

// ─── AT állapot és snapshot változók ────────────────────────
extern bool gAtStatusInProgress;
extern String gAtStatusSnapshot;
extern unsigned long gAtStatusSnapshotAt;

// ─── Serial log ─────────────────────────────────────────────
#define MLOG(x)   Serial.println(F("[MODEM] " x))
#define MLOGv(x)  Serial.println("[MODEM] " + String(x))
#define SMS_POLL_INTERVAL_MS 30000UL

void modemPowerOn();
void modemPowerOff();
String getNetType();
String modemGetTime();
String bestAvailableTimestamp();
void updateModemStats();
bool modemInit();

// ─── AT kommunikáció és diagnosztika ────────────────────────
String modemAtQuery(const String& cmd, unsigned long timeoutMs = 1200);
void refreshAtStatusSnapshot();

struct DataConnState {
  bool active = false;
  String ip = "";
  String lastError = "";
  bool inProgress = false;

  bool pingInProgress = false;
  String pingTarget = "";
  String pingResult = "";
  bool pingOk = false;
};

String modemApplyExpertConfig(const String& cnmp, const String& cgsms, const String& bands, const String& cmnb);
void modemResetExpertConfig();

extern DataConnState gData;

struct ReceivedSms {
  String sender;
  String timestamp;
  String ourTimestamp;
  String text;
  unsigned long receivedAt;
};

extern ReceivedSms gSmsInbox[SMS_INBOX_HARD_MAX];

extern int gSmsInboxCount;
extern int gSmsInboxHead;
extern int gSmsInboxLimit;
extern unsigned long gSmsTotalReceived;

extern unsigned long gLastSmsPoll;

struct LedConfig {
  uint8_t mode = DEFAULT_LED_MODE;
  uint8_t customPin = DEFAULT_CUSTOM_PIN;
  bool triggerOn = false;
  bool manualOverride = false;
};

extern LedConfig gLed;

String changeSIMPin(const String& oldPin, const String& newPin);

String getSmsc();
String setSmsc(const String& number);

String sendSMS(const String& number, const String& text);

String startCall(const String& number);
void hangUp();
void monitorCall();

String dataConnEnable();
String dataConnDisable();
String dataConnPing(const String& targetIp);

bool sendNtfyAlert(const String& message);

void saveSmsInboxLimit(int limit);
void loadSmsInboxLimit();

void smsInboxAdd(
    const String& sender,
    const String& timestamp,
    const String& text);

bool looksLikeUcs2Hex(const String& s);
String decodeUcs2Hex(const String& hex);
String autoDecodeSmsText(const String& raw);

void pollIncomingSms();
void smsInboxLoop();

void saveLedConfig();
void loadLedConfig();

int currentLedGpio();
void ledPinReinit();

void setNetLightAT(bool on);

void ledTrigger();
void ledSetAuto();