//modem_mgr.cpp

#include <LittleFS.h>
#include "modem_mgr.h"

unsigned long gSmsTotalReceived = 0;
unsigned long gLastSmsPoll = 0;

ReceivedSms gSmsInbox[SMS_INBOX_HARD_MAX];

int gSmsInboxCount = 0;
int gSmsInboxHead = 0;
int gSmsInboxLimit = SMS_INBOX_DEFAULT_LIMIT;
String gManualNetCode = "";

void diagAddWithTimestamp(const String& msg) {
  unsigned long ms = millis();
  unsigned long secs = ms / 1000;
  unsigned long centis = (ms % 1000) / 100;
  
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "[%lu.%lus] ", secs, centis);

  String timestampedMsg = String(timeBuf) + msg;
  diagAdd(timestampedMsg);
}

void modemPowerOn() {
  diagAddWithTimestamp(F("[MODEM] Bekapcsolas ellenorzese..."));
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);

  modemSerial.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(300); yield();
  if(modem.testAT(800L)) {
    diagAddWithTimestamp(F("[MODEM] Mar bekapcsolva es valaszol - PWRKEY-t nem nyulok hozza."));
    gModem.powered = true;
    return;
  }

  diagAddWithTimestamp(F("[MODEM] Nem valaszol, PWRKEY pulzus kuldese..."));
  digitalWrite(MODEM_PWRKEY, LOW);  delay(1200); yield(); 
  digitalWrite(MODEM_PWRKEY, HIGH);                     
  diagAddWithTimestamp(F("[MODEM] Varakozas az indulasra..."));
  delay(5000); yield();  
  gModem.powered = true;
}

void modemPowerOff() {
  modem.poweroff();
  gModem.powered   = false;
  gModem.ready     = false;
  gModem.registered= false;
}

// ─── Hálózat típus lekérdezés ───────────────────────────────
String getNetType() {
  modem.sendAT("+CNSMOD?");
  if(modem.waitResponse(2000L, GF("+CNSMOD:")) != 1){
    modem.waitResponse(); return "?";
  }
  String r = modemSerial.readStringUntil('\n'); r.trim();
  modem.waitResponse();
  int c = r.lastIndexOf(',');
  int nm = (c>=0) ? r.substring(c+1).toInt() : r.toInt();
  switch(nm){
    case 0: return "Nincs";
    case 1: return "GSM";
    case 2: return "GPRS";
    case 3: return "EGPRS";
    case 5: return "LTE-M";
    case 6: return "NB-IoT";
    default: return "Egyeb("+String(nm)+")";
  }
}

// ─── Halozati ido lekerdezese (AT+CCLK?) ────────────────────
String modemGetTime() {
  if(!gModem.ready) return "";
  modemDrain();
  modemSerial.println("AT+CCLK?");
  String resp = modemReadUntilFinal(2000);
  int p = resp.indexOf("+CCLK:");
  if(p < 0) return "";
  int q1 = resp.indexOf('"', p);
  int q2 = (q1 >= 0) ? resp.indexOf('"', q1 + 1) : -1;
  if(q1 < 0 || q2 < 0) return "";
  String raw = resp.substring(q1 + 1, q2); 
  if(raw.length() < 17) return "";
  String yy = raw.substring(0, 2);
  String mo = raw.substring(3, 5);
  String dd = raw.substring(6, 8);
  String hh = raw.substring(9, 11);
  String mi = raw.substring(12, 14);
  String ss = raw.substring(15, 17);
  return "20" + yy + "-" + mo + "-" + dd + " " + hh + ":" + mi + ":" + ss;
}

String bestAvailableTimestamp() {
  if(gTime.synced) {
    String t = formatLocalTime();
    if(t != "-") return t;
  }
  String mt = modemGetTime();
  if(mt.length() > 0) return mt;
  return "";
}

// ─── Frissíti a modem állapotát (loop-ban hívható) ──────────
unsigned long lastStatUpdate = 0;
void updateModemStats() {
  if(!gModem.ready) return;
  if(gModem.callActive) return; 
  if(millis() - lastStatUpdate < 15000) return;  
  lastStatUpdate = millis();
  gModem.signalQuality = modem.getSignalQuality();
  gModem.registered    = modem.isNetworkConnected();
  if(gModem.registered){
    String op = modem.getOperator();
    if(op == "21630" || op == "21625") op = "Telekom HU (" + op + ")";
    else if(op == "21601") op = "Telenor HU (" + op + ")";
    else if(op == "21670") op = "Vodafone HU (" + op + ")";
    
    gModem.operatorName = op;
    gModem.netType      = getNetType();
  }
}

// ─── Fő inicializálás ───────────────────────────────────────
bool modemInit() {
  gModem.initAttempts++;
  gModem.initInProgress = true;
  gModem.uartResponding = false;

  gModem.initPhase = "Soros port inditasa...";
  gModem.initPhaseNum = 1;
  modemSerial.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000); yield();

  gModem.initPhase = "Modem valaszkeszseg ellenorzese...";
  gModem.initPhaseNum = 2;
  bool uartOk = false;
  for(int i=0; i<3 && !uartOk; i++){
    uartOk = modem.testAT(2000L);
    yield();
  }
  gModem.uartResponding = uartOk;

  if(!uartOk) {
    gModem.lastError = "A modem nem valaszol semmilyen parancsra (UART/tap hiba).";
    diagAddWithTimestamp("[MODEM] " + gModem.lastError);
    gModem.initInProgress = false;
    gModem.initPhase = "Sikertelen: nincs UART valasz";
    return false;
  }

  diagAddWithTimestamp(F("[MODEM] UART valaszol, inicializalas..."));
  gModem.initPhase = "Modem szoftveres inicializalasa...";
  gModem.initPhaseNum = 3;
  
  if (!modem.init()) {
    diagAddWithTimestamp(F("[MODEM] Gyors init SIKERTELEN, teljes RESTART kovetkezik..."));
    gModem.initPhase = "Modem ujrainditasa (AT+CFUN reset)...";
    modem.restart();
    delay(5000); yield();  
  } else {
    delay(1000); yield();
  }

  gModem.initPhase = "SIM kartya ellenorzese...";
  gModem.initPhaseNum = 4;
  SimStatus ss = SIM_ERROR;
  for(int attempt=0; attempt<5; attempt++){
    ss = modem.getSimStatus();
    diagAddWithTimestamp("[MODEM] SIM status (" + String(attempt+1) + "): " + String((int)ss));
    if(ss != SIM_ERROR) break;
    gModem.initPhase = "SIM kartya ellenorzese... (" + String(attempt+1) + "/5 probalkozas)";
    delay(3000); yield();
  }

  delay(1000); yield();
  String ccid = modem.getSimCCID();
  diagAddWithTimestamp("[MODEM] CCID: " + ccid);

  String savedCCID = loadCCID();

  if(ss == SIM_LOCKED) {
    String pin = loadPin();
    if(pin.length() == 0) {
      gModem.lastError = "PIN szukseges, de nincs mentve.";
      diagAddWithTimestamp(F("[MODEM] Nincs mentett PIN."));
      gModem.initInProgress = false;
      gModem.initPhase = "Sikertelen: PIN hianyzik";
      return false;
    }

    gModem.initPhase = "SIM PIN feloldasa...";
    diagAddWithTimestamp(F("[MODEM] PIN feloldas..."));
    if(!modem.simUnlock(pin.c_str())) {
      gModem.lastError = "Hibas PIN kod a mentett ertekkel!";
      diagAddWithTimestamp(F("[MODEM] PIN SIKERTELEN!"));
      gModem.initInProgress = false;
      gModem.initPhase = "Sikertelen: hibas PIN";
      return false;
    }
    delay(3000); yield();

  } else if(ss == SIM_READY) {
    diagAddWithTimestamp(F("[MODEM] SIM kesz (nincs PIN)."));
  } else {
    String statusDesc;
    switch((int)ss){
      case 0: statusDesc = "A modem nem erzekeli a SIM kartyat."; break;
      default: statusDesc = "Ismeretlen SIM allapot (kod=" + String((int)ss) + ").";
    }
    gModem.lastError = statusDesc;
    diagAddWithTimestamp("[MODEM] " + statusDesc);
    gModem.initInProgress = false;
    gModem.initPhase = "Sikertelen: SIM hiba";
    return false;
  }

  if(ccid.length() > 0 && ccid != savedCCID) {
    saveCCID(ccid);
  }
  gModem.simCCID = (ccid.length() > 0) ? ccid : savedCCID;

  gModem.initPhase = "IMEI lekerdezese...";
  gModem.simIMEI = modem.getIMEI();

  gModem.initPhase = "Halozatra csatlakozas...";
  gModem.initPhaseNum = 5;
  
  // Gyorsított hálózatregisztráció ellenőrzés (ha van mentett kézi kód)
  bool netConnected = false;
  if (gManualNetCode.length() >= 5) {
    diagAddWithTimestamp("[MODEM] Gyors csatlakozás kézi hálózathoz: " + gManualNetCode);
    String manualCmd = "AT+COPS=1,2,\"" + gManualNetCode + "\"";
    modemSerial.println(manualCmd);
    String manualResp = modemReadUntilFinal(15000);
    if (manualResp.indexOf("OK") >= 0) {
      netConnected = modem.waitForNetwork(10000L);
    }
  }
  
  if (!netConnected) {
    diagAddWithTimestamp(F("[MODEM] Automatikus hálózatkeresés indítása (AT+COPS=0)..."));
    if(!modem.waitForNetwork(30000L)) {
      int csq = modem.getSignalQuality();
      gModem.lastError = "Nincs halozat (CSQ=" + String(csq) + ").";
      diagAddWithTimestamp("[MODEM] NINCS HALOZAT! " + gModem.lastError);
      gModem.initInProgress = false;
      gModem.initPhase = "Sikertelen: nincs halozat";
      return false;
    }
  }

  gModem.ready      = true;
  gModem.registered = true;
  gModem.pinOk      = true;
  
  String op = modem.getOperator();
  if(op == "21630" || op == "21625") op = "Telekom HU (" + op + ")";
  else if(op == "21601") op = "Telenor HU (" + op + ")";
  else if(op == "21670") op = "Vodafone HU (" + op + ")";
  gModem.operatorName = op;

  gModem.signalQuality= modem.getSignalQuality();
  gModem.netType      = getNetType();
  gModem.lastError    = "";

  diagAddWithTimestamp("[MODEM] OK – " + gModem.operatorName);

  modemSerial.println("AT+CLCC=1");
  delay(200);
  modem.sendAT("+CMGF=1");
  modem.waitResponse();
  modem.sendAT("+CGSMS=1");
  modem.waitResponse();
  modem.sendAT("+CNMI=2,1,0,0,0");
  modem.waitResponse();

// --- ÚJ: Hálózati idő szinkronizálás engedélyezése a GNSS/Modem számára ---
  modem.sendAT("+CLTS=1");
  modem.waitResponse();
  modem.sendAT("&W"); // Konfiguráció mentése a modem flash memóriájába
  modem.waitResponse();

  gModem.initInProgress = false;
  gModem.initPhase = "Kesz - csatlakozva";
  return true;
}

// --- SIM PIN modositas a SIM-en (AT+CPWD) -----------------------
String changeSIMPin(const String& oldPin, const String& newPin) {
  if(!gModem.powered) return "A modem nincs bekapcsolva.";
  if(!gModem.ready)   return "A modem nincs inicializalva.";

  modem.sendAT("+CPWD=\"SC\",\"" + oldPin + "\",\"" + newPin + "\"");
  int r = modem.waitResponse(10000L, GF("OK"), GF("ERROR"), GF("+CME ERROR"));

  if(r == 1) {
    savePin(newPin);
    diagAddWithTimestamp(F("[MODEM] SIM PIN megvaltoztatva."));
    return "";
  }

  if(r == 3) {
    String errLine = modemSerial.readStringUntil('\n');
    errLine.trim();
    modem.waitResponse();
    int code = -1;
    int colonPos = errLine.indexOf(':');
    if(colonPos >= 0) code = errLine.substring(colonPos+1).toInt();

    switch(code) {
      case 16: return "Hibas jelenlegi PIN kod.";
      case 3:  return "Nincs jogosultsag a muvelethez.";
      case 10: return "Nincs SIM kartya behelyezve.";
      case 12: return "A SIM PUK kodra van zarolva.";
      default: return "SIM hiba (CME ERROR " + String(code) + ").";
    }
  }

  modem.waitResponse();
  return "A modem nem valaszolt a PIN csere parancsra.";
}

// --- SMSC (SMS-kozpont) lekerdezese/beallitasa -------------------
String getSmsc() {
  if(!gModem.ready) return "";
  modemDrain();
  modemSerial.println("AT+CSCA?");
  String resp = modemReadUntilFinal(3000);

  gModem.smscChecked = true;
  gModem.smscCheckedAt = millis();

  int p = resp.indexOf("+CSCA:");
  if(p < 0) {
    gModem.smscNumber = "";
    return "";
  }
  int q1 = resp.indexOf('"', p);
  int q2 = (q1 >= 0) ? resp.indexOf('"', q1 + 1) : -1;
  if(q1 >= 0 && q2 > q1) {
    gModem.smscNumber = resp.substring(q1 + 1, q2);
  } else {
    gModem.smscNumber = "";
  }
  return gModem.smscNumber;
}

String setSmsc(const String& number) {
  if(!gModem.ready) return "A modem nincs inicializalva.";
  if(number.length() == 0) return "Az SMSC szam nem lehet ures.";

  modemDrain();
  modemSerial.println("AT+CSCA=\"" + number + "\"");
  String resp = modemReadUntilFinal(3000);

  if(resp.indexOf("OK") >= 0) {
    gModem.smscNumber = number;
    gModem.smscChecked = true;
    gModem.smscCheckedAt = millis();
    return "";
  }
  return "Az SMSC beallitasa sikertelen.";
}

// --- SMS kuldes -------------------------------------------------
String sendSMS(const String& number, const String& text) {
  if(!gModem.powered)  return "A modem nincs bekapcsolva.";
  if(!gModem.ready)    return "A modem nincs inicializalva.";
  if(!gModem.registered) return "Nincs halozati regisztracio.";
  if(number.length() == 0) return "Hianyzik a cimzett telefonszama.";
  if(text.length() == 0)   return "Az uzenet szovege ures.";

  String smsc = getSmsc();
  if(smsc.length() == 0) {
    return "Az SMS-kozpont (SMSC) szama nincs beallitva a SIM-en!";
  }

  modemDrain();
  modemSerial.println("AT+CMGF=1");
  String resp = modemReadUntilFinal(3000);
  if(resp.indexOf("OK") < 0) return "A modem nem valtott SMS szoveges modba.";

  modemDrain();
  modemSerial.println("AT+CSCS=\"GSM\"");
  modemReadUntilFinal(3000);

  modemDrain();
  modemSerial.println("AT+CMGS=\"" + number + "\"");
  resp = modemReadUntilFinal(10000, true);
  if(resp.indexOf('>') < 0) return "A modem nem adott '>' SMS promptot.";

  modemSerial.print(text);
  modemSerial.write((char)26);
  delay(5000);

  String raw = "";
  while (modemSerial.available()) {
    raw += (char)modemSerial.read();
  }
  resp = raw;

  if(resp.indexOf("OK") >= 0 || resp.indexOf("+CMGS:") >= 0) return "";

  int cms = parseAtErrorCode(resp, "+CMS ERROR");
  if(cms >= 0) return "SMS hiba (CMS ERROR " + String(cms) + "): " + cmsErrorText(cms);

  return "SMS kuldes timeout vagy ures modemvalasz.";
}

// --- Hívás indítás ------------------------------------------
String startCall(const String& number) {
  if(!gModem.powered)    return "A modem nincs bekapcsolva.";
  if(!gModem.ready)      return "A modem nincs inicializalva.";
  if(!gModem.registered) return "Nincs halozati regisztracio.";
  if(gModem.callActive)  return "Mar folyamatban van egy hivas.";
  if(number.length() == 0) return "Hianyzik a hivando telefonszam.";

  modemDrain();
  modemSerial.println("ATD" + number + ";");
  String resp = modemReadUntilFinal(8000);

  if(resp.indexOf("OK") >= 0) {
    gModem.callActive = true;
    gModem.ringCount  = 0;
    gModem.callStart  = millis();
    return "";
  }
  if(resp.indexOf("NO CARRIER") >= 0) return "A halozat azonnal bontotta a hivast (NO CARRIER).";
  if(resp.indexOf("BUSY") >= 0) return "A hivott szam foglalt.";
  return "Hivas inditasa sikertelen.";
}

void hangUp() {
  modem.sendAT("H");
  modem.waitResponse(3000L);
  gModem.callActive = false;
  gModem.ringCount  = 0;
}

void monitorCall() {
  if(!gModem.callActive) return;

  String line = "";
  while(modemSerial.available()) {
    char c = modemSerial.read();
    if(c == '\n') {
      line.trim();
      if(line.length() > 0) {
        if(line == "RING") {
          gModem.ringCount++;
          if(gModem.ringCount >= 3) { hangUp(); return; }
        }
        else if(line.startsWith("NO CARRIER") || line.startsWith("BUSY") ||
                line.startsWith("NO ANSWER")  || line.startsWith("ERROR")) {
          gModem.callActive = false; gModem.ringCount = 0;
        }
        else if(line.indexOf("+CLCC") >= 0 && line.indexOf(",0,") >= 0) {
          delay(500); hangUp(); return;
        }
      }
      line = "";
    } else if(c != '\r') line += c;
  }

  if(millis() - gModem.callStart > 60000UL) {
    hangUp();
  }
}

String dataConnEnable() {
  if (!gModem.ready) return "Modem nincs kesz.";
  
  gData.inProgress = true;
  gData.lastError = "";
  
  modem.gprsDisconnect();
  delay(500);
  
  bool success = modem.gprsConnect("internet", "", "");

  if (!success) {
    gData.lastError = "Adatkapcsolat sikertelen.";
    gData.active = false;
  } else {
    modem.sendAT("+CNACT=1,1");
    String resp = modemReadUntilFinal(3000);
    
    if (resp.indexOf("OK") >= 0 || resp.indexOf("+CNACT:") >= 0) {
      gData.ip = modem.localIP().toString();
      gData.active = true;
      diagAddWithTimestamp("Adatkapcsolat felepitve (CNACT OK). IP: " + gData.ip);
    } else {
      gData.lastError = "CNACT aktivalas sikertelen.";
      gData.active = false;
    }
  }
  
  gData.inProgress = false;
  return gData.lastError;
}

String dataConnPing(const String& targetIp) {
  gData.pingInProgress = true;
  gData.pingTarget = targetIp;
  gData.pingResult = "";
  gData.pingOk = false;

  if (!gData.active) {
    gData.pingResult = "Nincs adatkapcsolat.";
    gData.pingInProgress = false;
    return gData.pingResult;
  }

  modem.streamClear();
  modem.sendAT("+SNPDPID=1");
  modem.waitResponse(1000L); 
  
  modem.sendAT("+SNPING4=\"" + targetIp + "\",1,32,5000");
  
  if (modem.waitResponse(2000L) != 1) {
    modem.sendAT("+SNPDPID=0");
    modem.waitResponse(1000L);
    modem.sendAT("+SNPING4=\"" + targetIp + "\",1,32,5000");
    
    if (modem.waitResponse(2000L) != 1) {
      gData.pingResult = "Parancs hiba.";
      gData.pingInProgress = false;
      return gData.pingResult;
    }
  }

  if (modem.waitResponse(6000L, "+SNPING4: ") == 1) {
    String resp = modem.stream.readStringUntil('\n');
    resp.trim();
    if (resp.indexOf("ERR") == -1 && resp.length() > 5) {
       gData.pingOk = true;
       int lastComma = resp.lastIndexOf(',');
       if(lastComma > 0 && lastComma < (int)resp.length() - 1) {
           String timeStr = resp.substring(lastComma + 1);
           gData.pingResult = "Sikeres: " + timeStr + " ms";
       } else {
           gData.pingResult = "Sikeres!";
       }
    } else {
       gData.pingResult = "Timeout";
    }
  } else {
    gData.pingResult = "Sikertelen";
  }

  diagAddWithTimestamp("Ping " + targetIp + " -> " + gData.pingResult);
  gData.pingInProgress = false;
  return gData.pingResult;
}

String dataConnDisable() {
  if(!gModem.ready) return "A modem nincs inicializalva.";
  gData.inProgress = true;

  modemDrain();
  modemSerial.println("AT+CNACT=1,0");
  String resp = modemReadUntilFinal(5000);
  gData.inProgress = false;

  gData.active = false;
  gData.ip = "";

  if(resp.indexOf("OK") < 0) {
    gData.lastError = "Adatkapcsolat kikapcsolasa nem adott OK-t.";
    return gData.lastError;
  }
  gData.lastError = "";
  return "";
}

bool sendNtfyAlert(const String& message) {
  if (!gData.active) {
    String err = dataConnEnable();
    if (!gData.active) {
      diagAddWithTimestamp("Ntfy hiba: Nincs aktív adatkapcsolat.");
      return false;
    }
  }

  String serverToUse = gNtfyServer.length() > 0 ? gNtfyServer : "https://ntfy.sh";
  String topicToUse = gNtfyTopic.length() > 0 ? gNtfyTopic : "balazs_kaptar_riasztas";
  
  if (serverToUse.startsWith("http://")) {
    serverToUse.replace("http://", "https://");
  } else if (!serverToUse.startsWith("https://")) {
    serverToUse = "https://" + serverToUse;
  }

  String url = serverToUse;
  if (!url.endsWith("/")) url += "/";
  if (topicToUse.startsWith("/")) topicToUse = topicToUse.substring(1);
  url += topicToUse;

  diagAddWithTimestamp("HTTPS Ntfy küldés ide: " + url);
  modemDrain(50);

  modemSerial.println("AT+SHDISC");
  diagAddWithTimestamp("STEP SHDISC: " + modemReadUntilFinal(1000));

  modemSerial.println("AT+CSSLCFG=\"sslversion\",1,4");
  diagAddWithTimestamp("STEP SSLVER: " + modemReadUntilFinal(1000));

  modemSerial.println("AT+CSSLCFG=\"authmode\",1,0");
  diagAddWithTimestamp("STEP AUTHMODE: " + modemReadUntilFinal(1000));

  modemSerial.println("AT+SHCONF=\"URL\",\"" + url + "\"");
  diagAddWithTimestamp("STEP CONF URL: " + modemReadUntilFinal(1500));

  modemSerial.println("AT+SHCONF=\"BODYLEN\",1024");
  diagAddWithTimestamp("STEP CONF BODY: " + modemReadUntilFinal(1000));

  modemSerial.println("AT+SHCONF=\"HEADERLEN\",350");
  diagAddWithTimestamp("STEP CONF HDR: " + modemReadUntilFinal(1000));

  modemSerial.println("AT+SHCONF=\"SSLPAR\",1");
  diagAddWithTimestamp("STEP CONF SSL: " + modemReadUntilFinal(1000));

  modemSerial.println("AT+SHCONN");
  String rConn = modemReadUntilFinal(6000);
  diagAddWithTimestamp("STEP SHCONN válasz: [" + rConn + "]");
  
  if (rConn.indexOf("OK") < 0) {
    diagAddWithTimestamp("Ntfy hiba: SHCONN elszállt.");
    modemSerial.println("AT+SHDISC");
    modemReadUntilFinal(1000);
    return false;
  }

  int len = message.length();
  modemSerial.println("AT+SHSTATE=1");
  modemReadUntilFinal(1000);

  modemSerial.print("AT+SHDATA=");
  modemSerial.print(len);
  modemSerial.println(",10000");

  unsigned long start = millis();
  bool promptFound = false;
  while (millis() - start < 3000) {
    if (modemSerial.available()) {
      String line = modemSerial.readStringUntil('\n');
      if (line.indexOf("DOWNLOAD") >= 0) {
        promptFound = true;
        break;
      }
    }
    yield();
  }

  if (!promptFound) {
    diagAddWithTimestamp("Ntfy hiba: SHDATA DOWNLOAD prompt időtúllépés.");
    modemSerial.println("AT+SHDISC");
    return false;
  }

  modemSerial.print(message);
  delay(200);
  diagAddWithTimestamp("STEP SHDATA beíratás: " + modemReadUntilFinal(2000));

  modemSerial.println("AT+SHREQ=\"" + url + "\",2");
  String actionResp = modemReadUntilFinal(10000);
  diagAddWithTimestamp("STEP SHREQ válasz: [" + actionResp + "]");

  modemSerial.println("AT+SHDISC");
  modemReadUntilFinal(1500);

  bool success = (actionResp.indexOf("+SHREQ: \"POST\"") >= 0 && actionResp.indexOf(",200,") >= 0);
  if (!success) {
    diagAddWithTimestamp("Ntfy hiba: Nem érkezett 200-as válasz a SHREQ-re.");
  } else {
    diagAddWithTimestamp("HTTPS Ntfy üzenet sikeresen elküldve!");
  }
  return success;
}

void smsInboxAdd(const String& sender, const String& timestamp, const String& text) {
  gSmsInbox[gSmsInboxHead].sender       = sender;
  gSmsInbox[gSmsInboxHead].timestamp    = timestamp;
  gSmsInbox[gSmsInboxHead].ourTimestamp = bestAvailableTimestamp();
  gSmsInbox[gSmsInboxHead].text         = text;
  gSmsInbox[gSmsInboxHead].receivedAt   = millis();
  gSmsInboxHead = (gSmsInboxHead + 1) % gSmsInboxLimit;
  if(gSmsInboxCount < gSmsInboxLimit) gSmsInboxCount++;
  gSmsTotalReceived++;
}

bool looksLikeUcs2Hex(const String& s) {
  if(s.length() < 4 || s.length() % 4 != 0) return false;
  for(size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    bool hex = (c>='0'&&c<='9') || (c>='A'&&c<='F') || (c>='a'&&c<='f');
    if(!hex) return false;
  }
  return true;
}

String decodeUcs2Hex(const String& hex) {
  String out;
  out.reserve(hex.length() / 4);
  for(size_t i = 0; i + 3 < hex.length(); i += 4) {
    uint16_t code = (uint16_t)strtol(hex.substring(i, i+4).c_str(), nullptr, 16);
    if(code < 0x80) {
      out += (char)code; 
    } else if(code < 0x800) {
      out += (char)(0xC0 | (code >> 6));
      out += (char)(0x80 | (code & 0x3F));
    } else {
      out += (char)(0xE0 | (code >> 12));
      out += (char)(0x80 | ((code >> 6) & 0x3F));
      out += (char)(0x80 | (code & 0x3F));
    }
  }
  return out;
}

String autoDecodeSmsText(const String& raw) {
  if(looksLikeUcs2Hex(raw)) return decodeUcs2Hex(raw);
  return raw;
}

void pollIncomingSms() {
  if(!gModem.ready) return;

  modemDrain();
  modemSerial.println("AT+CMGL=\"REC UNREAD\"");
  String resp = modemReadUntilFinal(4000);

  if(resp.indexOf("+CMGL:") < 0) return; 

  int pos = 0;
  int foundIndexes[20]; int foundCount = 0; 

  while(true) {
    int hdrStart = resp.indexOf("+CMGL:", pos);
    if(hdrStart < 0) break;
    int hdrEnd = resp.indexOf('\n', hdrStart);
    if(hdrEnd < 0) break;
    String header = resp.substring(hdrStart, hdrEnd);
    header.trim();

    int nextHdr = resp.indexOf("+CMGL:", hdrEnd);
    int textEnd = (nextHdr >= 0) ? nextHdr : resp.length();
    String msgText = resp.substring(hdrEnd + 1, textEnd);
    msgText.trim();
    if(nextHdr < 0) {
      int okPos = msgText.lastIndexOf("OK");
      if(okPos >= 0 && okPos >= (int)msgText.length() - 4) {
        msgText = msgText.substring(0, okPos);
        msgText.trim();
      }
    }

    int idxComma1 = header.indexOf(',');
    int msgIndex = (idxComma1 > 0) ? header.substring(header.indexOf(':')+1, idxComma1).toInt() : -1;

    String parts[4]; int partCount = 0;
    int searchFrom = 0;
    while(partCount < 4) {
      int a = header.indexOf('"', searchFrom);
      if(a < 0) break;
      int b = header.indexOf('"', a + 1);
      if(b < 0) break;
      parts[partCount++] = header.substring(a + 1, b);
      searchFrom = b + 1;
    }
    String sender = "ismeretlen";
    String timestamp = "";
    for(int i = 0; i < partCount; i++) {
      bool looksLikeNumber = parts[i].length() > 0 && (parts[i][0] == '+' || isDigit(parts[i][0]));
      bool looksLikeDate = parts[i].indexOf('/') >= 0 || parts[i].indexOf(':') >= 0;
      if(looksLikeNumber && !looksLikeDate) {
        sender = parts[i];
      } else if(looksLikeDate) {
        timestamp = parts[i];
      }
    }
    if(timestamp.length() == 0 && partCount > 0) timestamp = parts[partCount - 1];

    if(msgIndex >= 0) {
      smsInboxAdd(autoDecodeSmsText(sender), timestamp, autoDecodeSmsText(msgText));
      if(foundCount < 20) foundIndexes[foundCount++] = msgIndex;
    }

    pos = textEnd;
  }

  for(int i = 0; i < foundCount; i++) {
    modemDrain();
    modemSerial.println("AT+CMGD=" + String(foundIndexes[i]));
    modemReadUntilFinal(2000);
    yield();
  }
}

#define SMS_POLL_INTERVAL_MS 30000UL
void smsInboxLoop() {
  if(!gModem.ready) return;
  if(gModem.callActive) return;               
  if(gSmsSendRequested || gSmsSendInProgress) return; 
  if(gModemInitRequested || gModem.initInProgress) return; 
  if(millis() - gLastSmsPoll < SMS_POLL_INTERVAL_MS) return;

  gLastSmsPoll = millis();
  pollIncomingSms();
}

void saveLedConfig() {
  EEPROM.write(ADDR_LED_MODE, gLed.mode);
  EEPROM.write(ADDR_LED_CUSTOM, gLed.customPin);
  EEPROM.commit();
}

void loadLedConfig() {
  uint8_t m = EEPROM.read(ADDR_LED_MODE);
  uint8_t p = EEPROM.read(ADDR_LED_CUSTOM);
  gLed.mode      = (m <= LED_MODE_AT_NETLIGHT) ? m : DEFAULT_LED_MODE;
  gLed.customPin = (p >= 2 && p <= 39) ? p : DEFAULT_CUSTOM_PIN;
}

int currentLedGpio() {
  switch(gLed.mode) {
    case LED_MODE_V10:    return LED_PIN_V10;
    case LED_MODE_V11:    return LED_PIN_V11;
    case LED_MODE_CUSTOM: return gLed.customPin;
    default:              return -1; 
  }
}

void ledPinReinit() {
  int pin = currentLedGpio();
  if(pin >= 0) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

void setNetLightAT(bool on) {
  modem.sendAT("+CNETLIGHT=" + String(on ? 1 : 0));
  modem.waitResponse(1000L);
}

void ledTrigger() {
  gLed.triggerOn = !gLed.triggerOn;
  gLed.manualOverride = true;
  if(gLed.mode == LED_MODE_AT_NETLIGHT) {
    setNetLightAT(gLed.triggerOn);
  } else {
    int pin = currentLedGpio();
    if(pin >= 0) {
      digitalWrite(pin, gLed.triggerOn ? HIGH : LOW);
    }
  }
}

void ledSetAuto() {
  gLed.manualOverride = false;
  gLed.triggerOn = false;
}

bool gAtStatusInProgress = false;
String gAtStatusSnapshot = "";
unsigned long gAtStatusSnapshotAt = 0;

String modemAtQuery(const String& cmd, unsigned long timeoutMs) {
  modemDrain(25);
  modemSerial.println(cmd);
  String resp = modemReadUntilFinal(timeoutMs);
  resp.trim();
  if(resp.length() == 0) resp = "(ures valasz / timeout)";
  return resp;
}

void refreshAtStatusSnapshot() {
  // A fejléc rövidítése
  gAtStatusSnapshot = "AT ÁLLAPOT SNAPSHOT | " + gTime.localTime + "\n";
  gAtStatusSnapshot += "=========================================================\n";

  // A parancsok tömbje (ez már megvan a kódodban)
  const char* cmds[] = {
    "AT", "ATI", "AT+CGMI", "AT+CGMM", "AT+CGMR", "AT+CGSN", "AT+CIMI", 
    "AT+CCID", "AT+CPIN?", "AT+CSQ", "AT+COPS?", "AT+CREG?", "AT+CGREG?", 
    "AT+CEREG?", "AT+CNSMOD?", "AT+CGATT?", "AT+CGACT?", "AT+CNACT?", 
    "AT+CGDCONT?", "AT+CSCA?", "AT+CMGF?", "AT+CSCS?", "AT+CNMI?", 
    "AT+CLCC", "AT+CCLK?", "AT+CBC", "AT+CGNSPWR?", "AT+CGNSINF", 
    "AT+CGNSSINFO", "AT+CGNSANT"
  };
  int cmdCount = sizeof(cmds) / sizeof(cmds[0]);

  for (int i = 0; i < cmdCount; i++) {
    String cmd = cmds[i];
    String resp = modemAtQuery(cmd, 1500); // Parancs küldése
    
    // --- TÖMÖRÍTÉS LOGIKÁJA ---
    
    // 1. Sortörések és kocsivisszák eltüntetése (szóközre cserélve)
    resp.replace("\r", "");
    resp.replace("\n", " ");
    resp.trim();
    
    // 2. Felesleges "OK" levágása a végéről, ha van előtte érdemi adat (pl. "+CSQ: 31,99 OK" -> "+CSQ: 31,99")
    if (resp.endsWith(" OK") && resp.length() > 3) {
      resp = resp.substring(0, resp.length() - 3);
    } else if (resp.endsWith("OK") && resp.length() > 2) {
      resp = resp.substring(0, resp.length() - 2);
    }
    resp.trim();
    
    // 3. Ha teljesen üres maradt (csak egy OK volt), visszaírjuk
    if (resp.length() == 0) resp = "OK";
    
    // 4. Szépen igazított, egysoros formázás: [01/30] AT+CSQ         -> +CSQ: 31,99
    char lineBuf[256];
    snprintf(lineBuf, sizeof(lineBuf), "[%02d/%02d] %-14s -> %s\n", i + 1, cmdCount, cmd.c_str(), resp.c_str());
    
    gAtStatusSnapshot += lineBuf;
  }
}

String modemApplyExpertConfig(const String& cnmp, const String& cgsms, const String& bands, const String& cmnb) {
  String log = "";
  modem.sendAT("+CFUN=0"); modem.waitResponse(2000L);

  if(cnmp.length()) {
    modem.sendAT("+CNMP=" + cnmp);
    log += "AT+CNMP=" + cnmp + " -> " + modemReadUntilFinal(2000) + "\n";
  }
  if(cgsms.length()) {
    modem.sendAT("+CGSMS=" + cgsms);
    log += "AT+CGSMS=" + cgsms + " -> " + modemReadUntilFinal(2000) + "\n";
  }
  if(bands.length()) {
    modem.sendAT("+CBANDCFG=\"CATM\"," + bands);
    log += "AT+CBANDCFG=\"CATM\"," + bands + " -> " + modemReadUntilFinal(2000) + "\n";
  }
  if(cmnb.length()) {
    modem.sendAT("+CMNB=" + cmnb);
    log += "AT+CMNB=" + cmnb + " -> " + modemReadUntilFinal(2000) + "\n";
  }

  modem.sendAT("+CFUN=1"); modem.waitResponse(3000L);
  log += "\nModem rádió újraindítva (+CFUN=1). OK!\n";
  
  return log;
}

void modemResetExpertConfig() {
  modem.sendAT("+CFUN=0"); modem.waitResponse(2000L);
  modem.sendAT("+CNMP=38"); modem.waitResponse(1000L);
  modem.sendAT("+CGSMS=1"); modem.waitResponse(1000L);
  modem.sendAT("+CMNB=1"); modem.waitResponse(1000L);
  modem.sendAT("+CBANDCFG=\"CATM\",3,8,20"); modem.waitResponse(1000L);
  modem.sendAT("+CFUN=1"); modem.waitResponse(3000L);
}

// Elérhető hálózatok listázása (AT+COPS=?)
String scanAvailableNetworks() {
  if(!gModem.ready) return "A modem nincs kész.";
  modemDrain();
  modemSerial.println("AT+COPS=?");
  String resp = modemReadUntilFinal(45000);
  resp.trim();
  return resp;
}

void loadNetConfig() {
    if (LittleFS.exists("/net.cfg")) {
        File f = LittleFS.open("/net.cfg", "r");
        if (f) {
            gManualNetCode = f.readStringUntil('\n');
            gManualNetCode.trim();
            f.close();
        }
    }
}

void saveNetConfig(const String& netCode) {
    gManualNetCode = netCode;
    File f = LittleFS.open("/net.cfg", "w");
    if (f) {
        f.println(gManualNetCode);
        f.close();
    }
}

String setManualNetwork(const String& code, int act) {
    if (code.length() < 5) return "Érvénytelen hálózati kód.";
    
    // AT+COPS=1 (kézi), 2 (numerikus formátum), "kód"
    String cmd = "AT+COPS=1,2,\"" + code + "\"";
    String resp = modemAtQuery(cmd, 35000);
    
    if (resp.indexOf("OK") != -1) {
        saveNetConfig(code); // Sikeres csatlakozás után mentjük
        return ""; 
    }
    return "A modem elutasította a kézi hálózatot.";
}

String setAutoNetwork() {
    String resp = modemAtQuery("AT+COPS=0", 35000);
    if (resp.indexOf("OK") != -1) {
        saveNetConfig(""); // Üres string jelzi az automatikus módot
        return "";
    }
    return "Hiba az automatikus mód visszaállításakor.";
}