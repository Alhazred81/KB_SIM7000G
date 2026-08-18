//modem_mgr.cpp

#include "modem_mgr.h"

unsigned long gSmsTotalReceived = 0;
unsigned long gLastSmsPoll = 0;

ReceivedSms gSmsInbox[SMS_INBOX_HARD_MAX];

int gSmsInboxCount = 0;
int gSmsInboxHead = 0;
int gSmsInboxLimit = SMS_INBOX_DEFAULT_LIMIT;

void modemPowerOn() {
  Serial.println(F("[MODEM] Bekapcsolas ellenorzese..."));
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); // nyugalmi szint biztositasa

  // FONTOS: a PWRKEY-pulzus egy MAR bekapcsolt SIM7000G-n TOGGLE-kent
  // viselkedik - azaz egy ujabb pulzus KIKAPCSOLJA a mar futo modemet!
  // Ha a modemSerial mar korabban inicializalva volt (pl. egy fizikai
  // reset gomb miatt fut), es ide masodszor is behivodik ez a fuggveny,
  // egy "vak" pulzus veletlenul kikapcsolna. Ezert eloszor gyorsan
  // ellenorizzuk, valaszol-e mar - ha igen, nem nyulunk a PWRKEY-hez.
  modemSerial.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(300); yield();
  if(modem.testAT(800L)) {
    Serial.println(F("[MODEM] Mar bekapcsolva es valaszol - PWRKEY-t nem nyulok hozza."));
    gModem.powered = true;
    return;
  }

  Serial.println(F("[MODEM] Nem valaszol, PWRKEY pulzus kuldese..."));
  // A SIM7000G PWRKEY nyugalmi allapotban HIGH (nincs nyomva a "gomb").
  // A tenyleges "gombnyomas" a LOW-ra huzas legalabb 1 masodpercre,
  // majd HIGH-ra engedes - ez pontosan emulalja a fizikai reset gombot.
  digitalWrite(MODEM_PWRKEY, LOW);  delay(1200); yield(); // "gombnyomas" (>=1s)
  digitalWrite(MODEM_PWRKEY, HIGH);                       // elengedes
  Serial.println(F("[MODEM] Varakozas a felallasra..."));
  delay(5000); yield();  // elég idő az inicializáláshoz, WDT táplálva
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
// Ez a modem sajat ora-erteke, amit tobbnyire a GSM halozat allit be
// automatikusan (NITZ) - ezert AP modban is elerheto, amikor NTP nem
// mukodhet (nincs internet), amig van GSM regisztracio. Formatum:
// +CCLK: "24/01/15,10:30:00+04" -> visszaadjuk "2024-01-15 10:30:00" alakban.
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
  String raw = resp.substring(q1 + 1, q2); // pl. "24/01/15,10:30:00+04"
  if(raw.length() < 17) return "";
  // YY/MM/DD,HH:MM:SS+TZ -> YYYY-MM-DD HH:MM:SS (a +TZ resztol eltekintunk,
  // mert a legtobb operator amugy is helyi idot ad vissza itt)
  String yy = raw.substring(0, 2);
  String mo = raw.substring(3, 5);
  String dd = raw.substring(6, 8);
  String hh = raw.substring(9, 11);
  String mi = raw.substring(12, 14);
  String ss = raw.substring(15, 17);
  // 2000+ feltetelezese - a GSM ora ket szamjegyes evet ad, 20xx-nek vesszuk
  return "20" + yy + "-" + mo + "-" + dd + " " + hh + ":" + mi + ":" + ss;
}

// Legjobb elerheto idobelyeg: eloszor a mi sajat NTP-szinkronunkat probalja
// (ha van WiFi/internet), kulonben a modem halozati ideit (AP modban is
// mukodhet, amig van GSM regisztracio). Ha egyik sem elerheto, ures stringet ad.
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
  if(gModem.callActive) return; // hívás közben ne zavarjuk a soros vonalat (URC-k elsőbbséget élveznek)
  if(millis() - lastStatUpdate < 15000) return;  // 15 mp-nként elég
  lastStatUpdate = millis();
  // Ezek AT parancsok, de rövid timeouttal
  gModem.signalQuality = modem.getSignalQuality();
  gModem.registered    = modem.isNetworkConnected();
  if(gModem.registered){
    gModem.operatorName = modem.getOperator();
    gModem.netType      = getNetType();
  }
}

// ─── Fő inicializálás ───────────────────────────────────────
// initPhase/initPhaseNum menet közben frissül, hogy egy másik
// webkérés (AJAX polling) is le tudja kérdezni, hol tart a folyamat -
// nem kell néma visszaszámlálóra hagyatkozni.
bool modemInit() {
  gModem.initAttempts++;
  gModem.initInProgress = true;
  gModem.uartResponding = false;

  gModem.initPhase = "Soros port inditasa...";
  gModem.initPhaseNum = 1;
  modemSerial.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000); yield();

  // ── UART-válaszkészség explicit ellenőrzése ──────────────
  // Ez különbözteti meg: "a modem egyáltalán nem válaszol semmire"
  // vs "a modem válaszol, csak épp a SIM-mel van baj".
  gModem.initPhase = "Modem valaszkeszseg ellenorzese...";
  gModem.initPhaseNum = 2;
  bool uartOk = false;
  for(int i=0; i<3 && !uartOk; i++){
    uartOk = modem.testAT(2000L);
    yield();
  }
  gModem.uartResponding = uartOk;

  if(!uartOk) {
    gModem.lastError = "A modem nem valaszol semmilyen parancsra (UART/tap hiba). "
                        "Ellenorizd: 1) a modem be van-e kapcsolva (PWRKEY), "
                        "2) elegendo aramellatas van-e (a SIM7000G csucsteljesitmenye ~2A, "
                        "USB port gyakran nem eleg), 3) a TX/RX bekotes helyes-e.";
    Serial.println("[MODEM] " + gModem.lastError);
    gModem.initInProgress = false;
    gModem.initPhase = "Sikertelen: nincs UART valasz";
    return false;
  }

  Serial.println(F("[MODEM] UART valaszol, restart..."));
  gModem.initPhase = "Modem ujrainditasa (AT+CFUN reset)...";
  gModem.initPhaseNum = 3;
  modem.restart();
  delay(5000); yield();  // több idő a SIM felállásához

  // SIM státusz – ha SIM_ERROR, várunk még és újrapróbáljuk
  gModem.initPhase = "SIM kartya ellenorzese...";
  gModem.initPhaseNum = 4;
  SimStatus ss = SIM_ERROR;
  for(int attempt=0; attempt<5; attempt++){
    ss = modem.getSimStatus();
    Serial.print(F("[MODEM] SIM status ("));
    Serial.print(attempt+1);
    Serial.print(F("): "));
    Serial.println((int)ss);
    if(ss != SIM_ERROR) break;
    gModem.initPhase = "SIM kartya ellenorzese... (" + String(attempt+1) + "/5 probalkozas)";
    Serial.println(F("[MODEM] SIM_ERROR, varakozas..."));
    delay(3000); yield();
  }

  // CCID lekérdezés (SIM_READY vagy SIM_LOCKED után)
  delay(1000); yield();
  String ccid = modem.getSimCCID();
  Serial.println("[MODEM] CCID: " + ccid);

  String savedCCID = loadCCID();

  if(ss == SIM_LOCKED) {
    // ── Új SIM kártya? ──────────────────────────────────────
    if(savedCCID.length() > 0 && savedCCID != ccid) {
      // Más SIM → nem próbálkozunk a régi PIN-nel
      Serial.println(F("[MODEM] ISMERETLEN SIM! Torlom a mentett PIN-t."));
      clearPin();
      clearCCID();
      gModem.lastError = "Ismeretlen SIM kartya! Uj PIN szukseges.";
      gModem.initInProgress = false;
      gModem.initPhase = "Sikertelen: ismeretlen SIM";
      return false;
    }

    String pin = loadPin();
    if(pin.length() == 0) {
      gModem.lastError = "PIN szukseges, de nincs mentve.";
      Serial.println(F("[MODEM] Nincs mentett PIN."));
      gModem.initInProgress = false;
      gModem.initPhase = "Sikertelen: PIN hianyzik";
      return false;
    }

    gModem.initPhase = "SIM PIN feloldasa...";
    Serial.println(F("[MODEM] PIN feloldas..."));
    if(!modem.simUnlock(pin.c_str())) {
      gModem.lastError = "Hibas PIN kod a mentett ertekkel! FIGYELEM: tobbszori hibas PIN "
                          "utan a SIM PUK-ra zarolhat. Ellenorizd/csereld a PIN-t a Beallitasoknal.";
      Serial.println(F("[MODEM] PIN SIKERTELEN!"));
      gModem.initInProgress = false;
      gModem.initPhase = "Sikertelen: hibas PIN";
      return false;
    }
    delay(3000); yield();

  } else if(ss == SIM_READY) {
    Serial.println(F("[MODEM] SIM kesz (nincs PIN)."));

    // Ha új SIM (más CCID) → töröljük a régi PIN-t
    if(savedCCID.length() > 0 && savedCCID != ccid && ccid.length() > 0) {
      Serial.println(F("[MODEM] Uj SIM eszlelve, regi PIN torolve."));
      clearPin();
    }
  } else {
    String statusDesc;
    switch((int)ss){
      case 0: statusDesc = "A modem valaszol, de nem erzekeli a SIM kartyat "
                            "(UART kapcsolat rendben, ez kifejezetten SIM-problema). "
                            "Ellenorizd, hogy jol van-e behelyezve a SIM, es hogy nem serult-e."; break;
      default: statusDesc = "Ismeretlen SIM allapot (kod=" + String((int)ss) + ").";
    }
    gModem.lastError = statusDesc;
    Serial.println("[MODEM] " + statusDesc);
    gModem.initInProgress = false;
    gModem.initPhase = "Sikertelen: SIM hiba";
    return false;
  }

  // CCID mentése ha szükséges
  if(ccid.length() > 0 && ccid != savedCCID) {
    saveCCID(ccid);
  }
  gModem.simCCID = ccid;

  // IMEI
  gModem.initPhase = "IMEI lekerdezese...";
  gModem.simIMEI = modem.getIMEI();

  // Hálózat
  gModem.initPhase = "Halozatra csatlakozas (akar 30 mp)...";
  gModem.initPhaseNum = 5;
  Serial.println(F("[MODEM] Halozat varas..."));
  if(!modem.waitForNetwork(30000L)) {
    int csq = modem.getSignalQuality();
    if(csq == 99 || csq == 0) {
      gModem.lastError = "Nincs halozat: a modem nem lat semmilyen jelet (CSQ=" + String(csq) +
                          "). Ellenorizd az antenna csatlakozasat.";
    } else {
      gModem.lastError = "Nincs halozat, pedig van jel (CSQ=" + String(csq) +
                          "). Lehet, hogy a SIM nincs aktivalva, vagy nincs lefedettseg ezen a helyen.";
    }
    Serial.println("[MODEM] NINCS HALOZAT! " + gModem.lastError);
    gModem.initInProgress = false;
    gModem.initPhase = "Sikertelen: nincs halozat";
    return false;
  }

  gModem.ready      = true;
  gModem.registered = true;
  gModem.pinOk      = true;
  gModem.operatorName = modem.getOperator();
  gModem.signalQuality= modem.getSignalQuality();
  gModem.netType      = getNetType();
  gModem.lastError    = "";

  Serial.println("[MODEM] OK – " + gModem.operatorName);

  // CLCC URC engedélyezése (hívás státusz)
  modemSerial.println("AT+CLCC=1");
  delay(200);
 // SMS szoveges mod
modem.sendAT("+CMGF=1");
modem.waitResponse();

// Telekom Domino + SIM7000G:
// SMS kuldes csak CS utvonalon mukodik stabilan.
modem.sendAT("+CGSMS=1");
modem.waitResponse();

// Bejovo SMS URC engedelyezese
modem.sendAT("+CNMI=2,1,0,0,0");
modem.waitResponse();

  gModem.initInProgress = false;
  gModem.initPhase = "Kesz - csatlakozva";
  return true;
}

// --- SIM PIN modositas a SIM-en (AT+CPWD) -----------------------
// Visszateres: "" = siker, egyebkent a hiba oka szovegesen
String changeSIMPin(const String& oldPin, const String& newPin) {
  if(!gModem.powered) return "A modem nincs bekapcsolva.";
  if(!gModem.ready)   return "A modem nincs inicializalva.";

  modem.sendAT("+CPWD=\"SC\",\"" + oldPin + "\",\"" + newPin + "\"");
  int r = modem.waitResponse(10000L, GF("OK"), GF("ERROR"), GF("+CME ERROR"));

  if(r == 1) {
    savePin(newPin);
    Serial.println(F("[MODEM] SIM PIN megvaltoztatva."));
    return "";
  }

  if(r == 3) {
    // +CME ERROR: <kod> - kiolvassuk a soros pufferbol
    String errLine = modemSerial.readStringUntil('\n');
    errLine.trim();
    modem.waitResponse();
    int code = -1;
    int colonPos = errLine.indexOf(':');
    if(colonPos >= 0) code = errLine.substring(colonPos+1).toInt();

    Serial.println("[MODEM] PIN csere CME ERROR: " + String(code));

    switch(code) {
      case 16: return "Hibas jelenlegi PIN kod. Ellenorizd, majd probald ujra.";
      case 3:  return "Nincs jogosultsag a muvelethez (operator zarolta?).";
      case 10: return "Nincs SIM kartya behelyezve.";
      case 12: return "A SIM PUK kodra van zarolva (tul sok hibas probalkozas). Csak SIM PUK-kal oldhato fel.";
      default: return "SIM hiba (CME ERROR " + String(code) + "). Reszletek: " + errLine;
    }
  }

  modem.waitResponse();
  Serial.println(F("[MODEM] PIN csere SIKERTELEN (nincs valasz)."));
  return "A modem nem valaszolt a PIN csere parancsra. Probald ujra, vagy ellenorizd a soros kapcsolatot.";
}

// --- SMSC (SMS-kozpont) lekerdezese/beallitasa -------------------
// A +CMS ERROR: 500 gyakori oka a hianyzo vagy hibas SMSC szam -
// ez a fuggveny lekerdezi az aktualisan beallitott erteket AT+CSCA?-val.
// Visszateres: az SMSC szam (pl. "+36309888000"), vagy "" ha nem sikerult.
String getSmsc() {
  if(!gModem.ready) return "";
  modemDrain();
  modemSerial.println("AT+CSCA?");
  String resp = modemReadUntilFinal(3000);
  Serial.println("[SMS] AT+CSCA? -> " + resp);

  gModem.smscChecked = true;
  gModem.smscCheckedAt = millis();

  int p = resp.indexOf("+CSCA:");
  if(p < 0) {
    gModem.smscNumber = "";
    return "";
  }
  // Formatum: +CSCA: "+36309888000",145
  int q1 = resp.indexOf('"', p);
  int q2 = (q1 >= 0) ? resp.indexOf('"', q1 + 1) : -1;
  if(q1 >= 0 && q2 > q1) {
    gModem.smscNumber = resp.substring(q1 + 1, q2);
  } else {
    gModem.smscNumber = "";
  }
  return gModem.smscNumber;
}

// SMSC manualis beallitasa - akkor kell, ha a getSmsc() ures eredmenyt ad,
// vagyis a SIM-en nincs elmentve az uzenetkozpont szama.
// Visszateres: "" = siker, egyebkent a hiba oka szovegesen.
String setSmsc(const String& number) {
  if(!gModem.ready) return "A modem nincs inicializalva.";
  if(number.length() == 0) return "Az SMSC szam nem lehet ures.";

  modemDrain();
  modemSerial.println("AT+CSCA=\"" + number + "\"");
  String resp = modemReadUntilFinal(3000);
  Serial.println("[SMS] AT+CSCA=... -> " + resp);

  if(resp.indexOf("OK") >= 0) {
    gModem.smscNumber = number;
    gModem.smscChecked = true;
    gModem.smscCheckedAt = millis();
    return "";
  }
  return "Az SMSC beallitasa sikertelen. Modem valasz: " + resp;
}

// --- SMS kuldes -------------------------------------------------
// Visszateres: "" = siker, egyebkent a hiba oka szovegesen
String sendSMS(const String& number, const String& text) {
  if(!gModem.powered)  return "A modem nincs bekapcsolva.";
  if(!gModem.ready)    return "A modem nincs inicializalva / nincs csatlakoztatva halozathoz.";
  if(!gModem.registered) return "Nincs halozati regisztracio - probald ujra par masodperc mulva. " + signalHint();
  if(number.length() == 0) return "Hianyzik a cimzett telefonszama.";
  if(text.length() == 0)   return "Az uzenet szovege ures.";

  Serial.println("[SMS] Kuldes indul: " + number + ", hossz=" + String(text.length()));
  Serial.println("[SMS] " + signalHint());

  // SMSC elovizsgalat - ha ures, ez a valoszinu oka a CMS 500-nak,
  // erdemes elore jelezni ahelyett hogy csak utolag derulne ki.
  String smsc = getSmsc();
  Serial.println("[SMS] SMSC: " + (smsc.length() ? smsc : String("(ures!)")));
  if(smsc.length() == 0) {
    return "Az SMS-kozpont (SMSC) szama nincs beallitva a SIM-en! Ez tipikus oka a "
           "'CMS ERROR 500' hibanak. Allitsd be az SMS oldalon (Telekom Domino "
           "eseten: +36309888000), majd probald ujra a kuldest.";
  }

  modemDrain();
  modemSerial.println("AT+CMGF=1");
  String resp = modemReadUntilFinal(3000);
  Serial.println("[SMS] AT+CMGF=1 -> " + resp);
  if(resp.indexOf("OK") < 0) {
    return "A modem nem valtott SMS szoveges modba. Modem valasz: " + resp;
  }

  modemDrain();
  modemSerial.println("AT+CSCS=\"GSM\"");
  resp = modemReadUntilFinal(3000);
  Serial.println("[SMS] AT+CSCS=GSM -> " + resp);

  modemDrain();
  modemSerial.println("AT+CMGS=\"" + number + "\"");
  resp = modemReadUntilFinal(10000, true);
  Serial.println("[SMS] AT+CMGS prompt -> " + resp);
  if(resp.indexOf('>') < 0) {
    int cms = parseAtErrorCode(resp, "+CMS ERROR");
    if(cms >= 0) return "SMS cimzett/parancs hiba (CMS ERROR " + String(cms) + "): " + cmsErrorText(cms) + " Valasz: " + resp;
    int cme = parseAtErrorCode(resp, "+CME ERROR");
    if(cme >= 0) return "Modem hiba a cimzett atadasakor (CME ERROR " + String(cme) + "). Valasz: " + resp;
    return "A modem nem adott '>' SMS promptot. Ez gyakran halozati/SIM/SMSC gond. Valasz: " + resp + " " + signalHint();
  }

  Serial.println("[SMS-TEXT] [" + text + "]");

  modemSerial.print(text);
modemSerial.write((char)26);

delay(5000);

String raw = "";
while (modemSerial.available()) {
  raw += (char)modemSerial.read();
}

Serial.println("======== SMS RAW ========");
Serial.println(raw);
Serial.println("=========================");
resp = raw;
  //resp = modemReadUntilFinal(60000);
  Serial.println("[SMS] Vegso modem valasz -> " + resp);

  if(resp.indexOf("OK") >= 0 || resp.indexOf("+CMGS:") >= 0) {
    Serial.println(F("[SMS] Kuldes sikeres."));
    return "";
  }

  int cms = parseAtErrorCode(resp, "+CMS ERROR");
  if(cms >= 0) {
    return "SMS hiba a halozattol (CMS ERROR " + String(cms) + "): " + cmsErrorText(cms) + " Valasz: " + resp;
  }
  int cme = parseAtErrorCode(resp, "+CME ERROR");
  if(cme >= 0) {
    return "Modem hiba SMS kuldes kozben (CME ERROR " + String(cme) + "). Valasz: " + resp;
  }
  if(resp.indexOf("ERROR") >= 0) {
    return "A modem ERROR valaszt adott SMS kuldes kozben. Valasz: " + resp + " " + signalHint();
  }
  return "SMS kuldes timeout vagy ures modemvalasz. Ellenorizd az SMSC-t (AT+CSCA?), az egyenleget, a szolgaltatoi SMS tiltast es a jelerosseget. " + signalHint();
}

// --- Hívás indítás ──────────────────────────────────────────
// Visszatérés: "" = siker, egyébként a hiba oka szövegesen
String startCall(const String& number) {
  if(!gModem.powered)    return "A modem nincs bekapcsolva.";
  if(!gModem.ready)      return "A modem nincs inicializalva / nincs csatlakoztatva halozathoz.";
  if(!gModem.registered) return "Nincs halozati regisztracio - probald ujra par masodperc mulva. " + signalHint();
  if(gModem.callActive)  return "Mar folyamatban van egy hivas, elobb bontsd azt.";
  if(number.length() == 0) return "Hianyzik a hivando telefonszam.";

  Serial.println("[MODEM] Hivas inditasa: " + number);
  Serial.println("[MODEM] " + signalHint());
  modemDrain();
  modemSerial.println("ATD" + number + ";");
  String resp = modemReadUntilFinal(8000);
  Serial.println("[MODEM] ATD valasz -> " + resp);

  if(resp.indexOf("OK") >= 0) {
    gModem.callActive = true;
    gModem.ringCount  = 0;
    gModem.callStart  = millis();
    Serial.println("[MODEM] Hivas inditva: " + number);
    return "";
  }
  if(resp.indexOf("NO CARRIER") >= 0) {
    String hint = "Gyakori ok: a SIM/adatcsomag nem enged hanghivast, nincs VoLTE/2G fallback, vagy a szam nem hivhato.";
    if(gModem.netType == "NB-IoT" || gModem.netType == "LTE-M") {
      hint = "A modem jelenleg " + gModem.netType + " halozattipuson van, ami TIPIKUSAN NEM TAMOGAT "
             "hanghivast (csak adatot) - ez a leggyakoribb oka ennek a hibanak IoT-SIM-eknel.";
    }
    return "A halozat azonnal bontotta a hivast (NO CARRIER). " + hint + " " + signalHint();
  }
  if(resp.indexOf("NO DIALTONE") >= 0) return "Nincs tarcsahang (NO DIALTONE). Ellenorizd a SIM-et, antennat es a halozati regisztraciot. " + signalHint();
  if(resp.indexOf("BUSY") >= 0) return "A hivott szam foglalt (BUSY).";
  if(resp.indexOf("NO ANSWER") >= 0) return "Nem volt valasz (NO ANSWER).";
  if(resp.indexOf("ERROR") >= 0) {
    String hint = "Ellenorizd, hogy a SIM tamogat-e hanghivast, es jo-e a szamformatum.";
    if(gModem.netType == "NB-IoT" || gModem.netType == "LTE-M") {
      hint = "A modem jelenleg " + gModem.netType + " halozattipuson van, ami TIPIKUSAN NEM TAMOGAT "
             "hanghivast (csak adatkapcsolatot) - ez varhato ok. Probald a Konfig oldalon "
             "atallitani a preferalt halozati modot, ha a modem tamogatja a 2G/GSM fallback-et.";
    }
    return "A modem ERROR valaszt adott a tarcsazasra. " + hint + " Jelenlegi halozattipus: " +
           (gModem.netType.length() ? gModem.netType : "ismeretlen") + ". Valasz: " + resp + " " + signalHint();
  }
  return "Hivas inditasa sikertelen: a modem nem adott ertelmezheto valaszt 8 masodpercen belul. Valasz: " + resp + " " + signalHint();
}

void hangUp() {
  modem.sendAT("H");
  modem.waitResponse(3000L);
  gModem.callActive = false;
  gModem.ringCount  = 0;
  Serial.println(F("[MODEM] Bontva."));
}

// ─── Hívás monitor (loop-ban) ───────────────────────────────
void monitorCall() {
  if(!gModem.callActive) return;

  String line = "";
  while(modemSerial.available()) {
    char c = modemSerial.read();
    if(c == '\n') {
      line.trim();
      if(line.length() > 0) {
        Serial.println("[URC] " + line);
        if(line == "RING") {
          gModem.ringCount++;
          Serial.println("[MODEM] Csengetes: " + String(gModem.ringCount));
          // 3. csengetes utan bontunk
          if(gModem.ringCount >= 3) { hangUp(); return; }
        }
        // Fogadva, foglalt, visszautasítva, vagy bármi más → bontás
        else if(line.startsWith("NO CARRIER") || line.startsWith("BUSY") ||
                line.startsWith("NO ANSWER")  || line.startsWith("ERROR")) {
          gModem.callActive = false; gModem.ringCount = 0;
          Serial.println("[MODEM] Hivas vege: " + line);
        }
        // Fogadta a hívott fél (+CLCC active state)
        else if(line.indexOf("+CLCC") >= 0 && line.indexOf(",0,") >= 0) {
          delay(500); hangUp(); return;
        }
      }
      line = "";
    } else if(c != '\r') line += c;
  }

  // Biztonsági timeout: 60 mp
  if(millis() - gModem.callStart > 60000UL) {
    Serial.println(F("[MODEM] Hivas timeout."));
    hangUp();
  }
}

// Explicit hivatkozás a diagnosztikai naplózóra
extern void diagAdd(const String& msg);

String dataConnEnable() {
  MLOG("Adatkapcsolat bekapcsolasa inditva...");
  if (!gModem.ready) {
    MLOG("Hiba: Modem nincs kesz.");
    return "Modem nincs kesz.";
  }
  
  gData.inProgress = true;
  gData.lastError = "";
  
  MLOG("Korabbi PDP kontextus lezarasa (gprsDisconnect)...");
  modem.gprsDisconnect();
  delay(500);
  
  MLOG("Csatlakozas az APN-hez (internet)...");
  bool success = modem.gprsConnect("internet", "", "");

  if (!success) {
    MLOG("Hiba: Adatkapcsolat felépítése sikertelen.");
    gData.lastError = "Adatkapcsolat sikertelen. (Idotullepes)";
    gData.active = false;
  } else {
    gData.ip = modem.localIP().toString();
    gData.active = true;
    MLOGv("Adatkapcsolat sikeresen felepitve. IP: " + gData.ip);
    diagAdd("Adatkapcsolat felepitve. IP: " + gData.ip);
  }
  
  gData.inProgress = false;
  return gData.lastError;
}

String dataConnPing(const String& targetIp) {
  MLOGv("Ping teszt inditva cel: " + targetIp);
  gData.pingInProgress = true;
  gData.pingTarget = targetIp;
  gData.pingResult = "";
  gData.pingOk = false;

  if (!gData.active) {
    MLOG("Hiba: Nincs aktiv adatkapcsolat a pinghez.");
    gData.pingResult = "Nincs adatkapcsolat.";
    gData.pingInProgress = false;
    return gData.pingResult;
  }

  MLOG("ICMP csomag kuldese (AT+SNPING4)...");
  
  // Tiszta puffer
  modem.streamClear();
  
  // Kiválasztjuk a hálózati profilt (a TinyGSM a SIM7000-nél az 1-es profilt, vagy a 0-st használja)
  // Próbáljuk beállítani a profilt. Ha hibát ad, nem gond, megyünk tovább.
  modem.sendAT("+SNPDPID=1");
  modem.waitResponse(1000L); 
  
  // Parancs: AT+SNPING4="ip",1,32,5000 
  // (1 db ping, 32 byte adat, 5000ms = 5 sec timeout)
  modem.sendAT("+SNPING4=\"" + targetIp + "\",1,32,5000");
  
  // Várjuk a parancs nyugtázását ("OK")
  if (modem.waitResponse(2000L) != 1) {
    // Ha nem fogadta el, próbáljuk meg a 0-s profillal!
    modem.sendAT("+SNPDPID=0");
    modem.waitResponse(1000L);
    modem.sendAT("+SNPING4=\"" + targetIp + "\",1,32,5000");
    
    if (modem.waitResponse(2000L) != 1) {
      gData.pingResult = "Parancs hiba (AT+SNPING4 nem tamogatott?)";
      MLOGv(gData.pingResult);
      gData.pingInProgress = false;
      return gData.pingResult;
    }
  }

  // Várunk a modem aszinkron válaszára, ami a "+SNPING4: " felirattal érkezik (max 6 sec)
  if (modem.waitResponse(6000L, "+SNPING4: ") == 1) {
    String resp = modem.stream.readStringUntil('\n');
    resp.trim();
    MLOGv("Ping nyers valasz: " + resp);
    
    // A válasz formátuma sikeres esetben: 1,8.8.8.8,124 (a 124 a válaszidő ms-ban)
    // Hiba esetén általában: 1,8.8.8.8,ERR vagy timeout
    if (resp.indexOf("ERR") == -1 && resp.length() > 5) {
       gData.pingOk = true;
       // Kinyerjük az utolsó számot (a válaszidőt) a szebb kiíratáshoz
       int lastComma = resp.lastIndexOf(',');
       if(lastComma > 0 && lastComma < (int)resp.length() - 1) {
           String timeStr = resp.substring(lastComma + 1);
           gData.pingResult = "Sikeres: " + timeStr + " ms";
       } else {
           gData.pingResult = "Sikeres! Nyers adat: " + resp;
       }
    } else {
       gData.pingResult = "Sikertelen vagy blokkolt (Timeout)";
    }
  } else {
    gData.pingResult = "Sikertelen (Nincs valasz a halozattol)";
    MLOG("Ping sikertelen (Idotullepes)");
  }

  diagAdd("Ping " + targetIp + " -> " + gData.pingResult);
  gData.pingInProgress = false;
  return gData.pingResult;
}

// ─── Adatkapcsolat kikapcsolasa (AT+CNACT=1,0) ──────────────
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
    gData.lastError = "Adatkapcsolat kikapcsolasa nem adott OK-t, de az allapotot lokalisan inaktivnak jeloljuk. Valasz: " + resp;
    return gData.lastError;
  }
  gData.lastError = "";
  Serial.println(F("[DATA] Adatkapcsolat kikapcsolva."));
  return "";
}

// ─── Ping teszt (AT+SNPING4) ─────────────────────────────────
// Csak akkor van ertelme, ha mar aktiv az adatkapcsolat. Visszaadja
// a szoveges eredmenyt ("" hiba eseten a gData.lastError-ben van),
// es beallitja gData.pingResult/pingOk-ot is a UI szamara.

// ─── HTTP (Ntfy) Riasztás beküldése ───────────────────────────
bool sendNtfyAlert(const String& message) {
  if (!gData.active) {
    String err = dataConnEnable();
    if (!gData.active) {
      Serial.println("[NTFY] Hiba: Nincs adatkapcsolat a riasztashoz. " + err);
      return false;
    }
  }

  modemDrain(50);

  modemSerial.println("AT+HTTPINIT");
  String r1 = modemReadUntilFinal(1500);
  if (r1.indexOf("ERROR") >= 0) {
    modemSerial.println("AT+HTTPTERM");
    modemReadUntilFinal(1000);
    modemSerial.println("AT+HTTPINIT");
    modemReadUntilFinal(1500);
  }

  modemSerial.println("AT+HTTPPARA=\"URL\",\"http://ntfy.sh/balazs_kaptar_riasztas\"");
  modemReadUntilFinal(1500);

  int len = message.length();
  modemSerial.print("AT+HTTPDATA=");
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
    Serial.println("[NTFY] Hiba: Nincs DOWNLOAD prompt.");
    modemSerial.println("AT+HTTPTERM");
    modemReadUntilFinal(1000);
    return false;
  }

  modemSerial.print(message);
  delay(200);
  modemReadUntilFinal(2000);

  modemSerial.println("AT+HTTPACTION=1");
  String actionResp = modemReadUntilFinal(5000);

  modemSerial.println("AT+HTTPTERM");
  modemReadUntilFinal(1500);

  bool success = (actionResp.indexOf("+HTTPACTION: 1,200") >= 0);
  if (success) {
    Serial.println("[NTFY] Riasztas sikeresen elkuldve.");
  } else {
    Serial.println("[NTFY] Riasztas HTTP hiba: " + actionResp);
  }

  return success;
}

// EEPROM-ba mentes/betoltes a tenyleges limithez
void saveSmsInboxLimit(int limit) {
  if(limit < 1) limit = 1;
  if(limit > SMS_INBOX_HARD_MAX) limit = SMS_INBOX_HARD_MAX;
  EEPROM.write(ADDR_SMS_INBOX_LIMIT, (uint8_t)limit);
  EEPROM.commit();
  gSmsInboxLimit = limit;
  // Limit-csokkentesnel/-novelesnel a legegyszerubb es legbiztonsagosabb,
  // ha nullazzuk a puffer-mutatokat - igy nem maradnak "arva" bejegyzesek
  // olyan indexeken, amik mar kivul esnek az uj limiten.
  gSmsInboxCount = 0;
  gSmsInboxHead  = 0;
}

void loadSmsInboxLimit() {
  uint8_t v = EEPROM.read(ADDR_SMS_INBOX_LIMIT);
  if(v == 0 || v == 0xFF || v > SMS_INBOX_HARD_MAX) {
    gSmsInboxLimit = SMS_INBOX_DEFAULT_LIMIT;
  } else {
    gSmsInboxLimit = v;
  }
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
  Serial.println("[SMS-RX] Uj uzenet: " + sender + " (" + timestamp + "): " + text.substring(0, 60));
}

// ─── UCS2 (hexa-kodolt UTF-16BE) SMS-szoveg felismerese/dekodolasa ─
// A modem a bejovo SMS szoveget (es nemelykor a felado szamat is)
// UCS2-ben adhatja vissza, ha az uzenet ekezetes/nemzeti karaktereket
// tartalmazott - ilyenkor a valasz csupa hexa-szamjegybol all, minden
// karakter 4 hexa-karakteren (2 byte, UTF-16BE). Ezt ismerjuk fel es
// dekodoljuk emberi olvashato szovegge, ahelyett hogy nyersen mutatnank.
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
      out += (char)code; // ASCII tartomany, kozvetlenul irhato
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

// Automatikusan felismeri es dekodolja, ha UCS2-hexa formatumu; kulonben
// valtozatlanul adja vissza (ez normal GSM/ASCII szoveg lehet).
String autoDecodeSmsText(const String& raw) {
  if(looksLikeUcs2Hex(raw)) return decodeUcs2Hex(raw);
  return raw;
}

// Egyetlen lekerdezesi kor: elolvassa az osszes olvasatlan SMS-t,
// eltarolja oket, majd torli a SIM-rol. Nem blokkol tul sokaig
// (max ~4mp), de MEGIS blokkolo AT-tranzakcio, ezert csak akkor
// szabad hivni, ha semmi mas nem hasznalja eppen a modemet.
void pollIncomingSms() {
  if(!gModem.ready) return;

  modemDrain();
  modemSerial.println("AT+CMGL=\"REC UNREAD\"");
  String resp = modemReadUntilFinal(4000);

  if(resp.indexOf("+CMGL:") < 0) return; // nincs uj uzenet, vagy hiba - csendben kilepunk

  // Soronkenti feldolgozas: minden "+CMGL: ..." fejlec sort követi
  // a kovetkezo sorban (vagy sorokban, ha tobbsoros az uzenet) a szoveg.
  int pos = 0;
  int foundIndexes[20]; int foundCount = 0; // a SIM-en levo index-ek a torleshez

  while(true) {
    int hdrStart = resp.indexOf("+CMGL:", pos);
    if(hdrStart < 0) break;
    int hdrEnd = resp.indexOf('\n', hdrStart);
    if(hdrEnd < 0) break;
    String header = resp.substring(hdrStart, hdrEnd);
    header.trim();

    // Kovetkezo +CMGL vagy OK/vege, hogy tudjuk hol all meg a szoveg-resz
    int nextHdr = resp.indexOf("+CMGL:", hdrEnd);
    int textEnd = (nextHdr >= 0) ? nextHdr : resp.length();
    String msgText = resp.substring(hdrEnd + 1, textEnd);
    msgText.trim();
    // Az utolso uzenetnel nincs kovetkezo +CMGL fejlec, ezert a szoveg
    // vegen ott marad a modem valasz zaro "OK" sora - levagjuk.
    if(nextHdr < 0) {
      int okPos = msgText.lastIndexOf("OK");
      if(okPos >= 0 && okPos >= (int)msgText.length() - 4) {
        msgText = msgText.substring(0, okPos);
        msgText.trim();
      }
    }

    // Fejlec parse: +CMGL: <index>,"REC UNREAD","<sender>",,"<timestamp>"
    int idxComma1 = header.indexOf(',');
    int msgIndex = (idxComma1 > 0) ? header.substring(header.indexOf(':')+1, idxComma1).toInt() : -1;

    // Az idezojelek kozotti szakaszokat sorrendben szedjuk ki - ez
    // robusztusabb, mint fix pozicio szerinti indexeles, mert a modem
    // ures mezoket (pl. alt-nev) idezojel nelkul hagyhat ki.
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
    // parts[0]="REC UNREAD", parts[1]=sender, parts[2] lehet ures (alt-nev), parts[3]=timestamp
    // FONTOS: a sender es a timestamp mezo is allhat csupa szamjegybol es
    // elvalaszto karakterekbol, ezert egy egyszeru "szamjeggyel kezdodik"
    // teszt osszekeverhetne oket (pl. a timestamp "24/01/15,10:30:00+08"
    // is szamjeggyel kezdodik). A megbizhato megkulonboztetes: a timestamp
    // MINDIG tartalmaz '/' vagy ':' karaktert (datum/ido elvalasztok),
    // a telefonszam SOHA. Ezert csak azt a reszt fogadjuk el feladokent,
    // ami '+' jellel vagy szamjeggyel kezdodik ES nem tartalmaz '/' vagy ':'-t.
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
    // Ha a fenti pontos felismeres nem talalt idobelyeget (pl. szokatlan
    // formatum), tartalek megoldaskent az utolso mezot hasznaljuk.
    if(timestamp.length() == 0 && partCount > 0) timestamp = parts[partCount - 1];

    if(msgIndex >= 0) {
      smsInboxAdd(autoDecodeSmsText(sender), timestamp, autoDecodeSmsText(msgText));
      if(foundCount < 20) foundIndexes[foundCount++] = msgIndex;
    }

    pos = textEnd;
  }

  // Torles a SIM-rol, hogy legkozelebb ne jelenjenek meg ujra "unread"-kent.
  for(int i = 0; i < foundCount; i++) {
    modemDrain();
    modemSerial.println("AT+CMGD=" + String(foundIndexes[i]));
    modemReadUntilFinal(2000);
    yield();
  }
}

// Periodikus hivo - csak akkor fut le tenylegesen, ha eltelt az
// intervallum ES senki mas nem hasznalja eppen a modemet.
#define SMS_POLL_INTERVAL_MS 30000UL
void smsInboxLoop() {
  if(!gModem.ready) return;
  if(gModem.callActive) return;              // hivas alatt ne
  if(gSmsSendRequested || gSmsSendInProgress) return; // kuldes alatt/varakozva ne
  if(gModemInitRequested || gModem.initInProgress) return; // modem-init alatt ne
  if(millis() - gLastSmsPoll < SMS_POLL_INTERVAL_MS) return;

  gLastSmsPoll = millis();
  pollIncomingSms();
}

// ════════════════════════════════════════════════════════════
//  LED / Panelverzió kezelés
// ════════════════════════════════════════════════════════════


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

// Az aktuális GPIO pin a mód alapján (AT módban nincs értelme)
int currentLedGpio() {
  switch(gLed.mode) {
    case LED_MODE_V10:    return LED_PIN_V10;
    case LED_MODE_V11:    return LED_PIN_V11;
    case LED_MODE_CUSTOM: return gLed.customPin;
    default:              return -1; // AT mód
  }
}

// GPIO pin újrainicializálása módváltás után
void ledPinReinit() {
  int pin = currentLedGpio();
  if(pin >= 0) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

// AT+CNETLIGHT / AT+SLEDS – hálózati LED be/ki (SIM7000 specifikus)
// A SIM7000 sorozat az AT+CNETLIGHT paranccsal engedi/tiltja a saját
// hálózati LED-jét (0=ki, 1=be)
void setNetLightAT(bool on) {
  modem.sendAT("+CNETLIGHT=" + String(on ? 1 : 0));
  modem.waitResponse(1000L);
}

// ─── Trigger gomb: toggle, allapot megmarad kikapcsolasig ────
// A gomb barmelyik allapota (be VAGY ki) MANUALIS felulbiralasnak szamit -
// ez kikapcsolja az automatikus halozat/GNSS-villogo mintat, amig a
// felhasznalo explicit vissza nem valt "Automatikus" modra.
void ledTrigger() {
  gLed.triggerOn = !gLed.triggerOn;
  gLed.manualOverride = true;
  if(gLed.mode == LED_MODE_AT_NETLIGHT) {
    setNetLightAT(gLed.triggerOn);
    Serial.println(gLed.triggerOn ? "[LED] AT halozati LED: BE (manualis)" : "[LED] AT halozati LED: KI (manualis)");
  } else {
    int pin = currentLedGpio();
    if(pin >= 0) {
      digitalWrite(pin, gLed.triggerOn ? HIGH : LOW);
      Serial.println(gLed.triggerOn ? "[LED] GPIO"+String(pin)+": BE (manualis)" : "[LED] GPIO"+String(pin)+": KI (manualis)");
    }
  }
}

// ─── Vissza automatikus (halozat/GNSS) villogo modba ────────
void ledSetAuto() {
  gLed.manualOverride = false;
  gLed.triggerOn = false;
  Serial.println(F("[LED] Vissza automatikus villogo modba."));
}

// ─── AT Diagnosztika változók ─────────────────────────────────
bool gAtStatusInProgress = false;
String gAtStatusSnapshot = "";
unsigned long gAtStatusSnapshotAt = 0;

// ─── AT Diagnosztika függvények ───────────────────────────────
String modemAtQuery(const String& cmd, unsigned long timeoutMs) {
  modemDrain(25);
  modemSerial.println(cmd);
  String resp = modemReadUntilFinal(timeoutMs);
  resp.trim();
  if(resp.length() == 0) resp = "(ures valasz / timeout)";
  return resp;
}

void refreshAtStatusSnapshot() {
  static const char* cmds[] = {
    "AT", "ATI", "AT+CGMI", "AT+CGMM", "AT+CGMR", "AT+CGSN", "AT+CIMI", "AT+CCID",
    "AT+CPIN?", "AT+CSQ", "AT+COPS?", "AT+CREG?", "AT+CGREG?", "AT+CEREG?", "AT+CNSMOD?",
    "AT+CGATT?", "AT+CGACT?", "AT+CNACT?", "AT+CGDCONT?", "AT+CSCA?", "AT+CMGF?", "AT+CSCS?",
    "AT+CNMI?", "AT+CLCC", "AT+CCLK?", "AT+CBC", "AT+CGNSPWR?", "AT+CGNSINF", "AT+CGNSSINFO", "AT+CGNSANT"
  };
  static const uint16_t timeouts[] = {
    800, 1200, 1000, 1000, 1000, 1000, 1200, 1200,
    1200, 1000, 1800, 1000, 1000, 1000, 1200,
    1200, 1200, 1800, 1400, 1500, 1000, 1000,
    1000, 1000, 1200, 1200, 1200, 1800, 1800, 1200
  };
  const uint8_t count = sizeof(cmds) / sizeof(cmds[0]);

  gAtStatusInProgress = true;
  gAtStatusSnapshot = "========================================\n";
  gAtStatusSnapshot += "        AT ALLAPOT SNAPSHOT             \n";
  gAtStatusSnapshot += "========================================\n";
  gAtStatusSnapshot += "Ido: " + bestAvailableTimestamp() + "\n";
  gAtStatusSnapshot += "========================================\n\n";
  gAtStatusSnapshot.reserve(6000);

  for(uint8_t i = 0; i < count; i++) {
    String cmd = cmds[i];
    String resp = modemAtQuery(cmd, timeouts[i]);
    
    resp.replace("\r", "");
    while(resp.indexOf("\n\n") >= 0) {
      resp.replace("\n\n", "\n");
    }
    resp.trim();

    char numBuf[12];
    snprintf(numBuf, sizeof(numBuf), "[%02d/%02d] ", i + 1, count);

    gAtStatusSnapshot += String(numBuf) + cmd + "\n";
    gAtStatusSnapshot += "----------------------------------------\n";
    gAtStatusSnapshot += (resp.length() > 0 ? resp : "(ures valasz / timeout)") + "\n\n";
    yield();
  }

  gAtStatusSnapshotAt = millis();
  gAtStatusInProgress = false;
}

// ─── Expert Konfiguráció ──────────────────────────────────────

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