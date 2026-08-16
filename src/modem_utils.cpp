//modem_utils.cpp

#include "modem_utils.h"
#include "modem_mgr.h"

void modemDrain(unsigned long ms) {
  unsigned long start = millis();
  while(millis() - start < ms) {
    while(modemSerial.available()) modemSerial.read();
    yield();
  }
}

String modemReadUntilFinal(unsigned long timeoutMs, bool stopAtPrompt) {
  String resp = "";
  unsigned long start = millis();
  unsigned long lastByte = millis();
  while(millis() - start < timeoutMs) {
    while(modemSerial.available()) {
      char c = (char)modemSerial.read();
      resp += c;
      lastByte = millis();
    }
    if(stopAtPrompt && resp.indexOf('>') >= 0) break;
    if(resp.indexOf("\r\nOK\r\n") >= 0 || resp.indexOf("\nOK\r\n") >= 0 ||
       resp.indexOf("\r\nERROR\r\n") >= 0 || resp.indexOf("\nERROR\r\n") >= 0 ||
       resp.indexOf("+CMS ERROR") >= 0 || resp.indexOf("+CME ERROR") >= 0 ||
       resp.indexOf("NO CARRIER") >= 0 || resp.indexOf("BUSY") >= 0 ||
       resp.indexOf("NO ANSWER") >= 0 || resp.indexOf("NO DIALTONE") >= 0) break;
    yield();
  }
  resp.trim();
  return resp;
}

int parseAtErrorCode(const String& resp, const String& token) {
  int p = resp.indexOf(token);
  if(p < 0) return -1;
  int c = resp.indexOf(':', p);
  if(c < 0) return -1;
  return resp.substring(c + 1).toInt();
}

String cmsErrorText(int code) {
  switch(code) {
    case 8: return "Operator altal tiltott SMS kuldes.";
    case 21: return "Rovid uzenet atvitele elutasitva.";
    case 27: return "Celallomas nem mukodik vagy nem elerheto.";
    case 28: return "Ervenytelen vagy hibas telefonszam-formatum.";
    case 38: return "Halozati hiba.";
    case 41: return "Atmeneti halozati hiba.";
    case 42: return "Halozati torlodas.";
    case 50: return "A kert SMS szolgaltatas nincs elofizetve.";
    case 95: return "Ervenytelen uzenet.";
    case 96: return "Kotelezo informacio hianyzik.";
    case 111: return "Protokollhiba.";
    case 300: return "ME hiba: a modem nem tudta vegrehajtani a muveletet.";
    case 302: return "Muvelet nem engedelyezett.";
    case 304: return "Ervenytelen PDU parameter.";
    case 305: return "Ervenytelen szoveges mod parameter.";
    case 310: return "Nincs SIM kartya.";
    case 311: return "SIM PIN szukseges.";
    case 313: return "SIM hiba.";
    case 314: return "SIM foglalt.";
    case 316: return "SIM PUK szukseges.";
    case 320: return "Memoria hiba.";
    case 322: return "Memoria megtelt.";
    case 330: return "SMSC cim ismeretlen.";
    case 331: return "Nincs halozati szolgaltatas.";
    case 332: return "Halozati timeout.";
    case 500: return "Ismeretlen modemhiba (generikus CMS 500-as kod). Ez a gyakorlatban "
                      "leggyakrabban az SMSC (SMS-kozpont) szamaval fugg ossze - vagy hianyzik "
                      "a SIM-en, vagy hibas formatumu. Ellenorizd az SMSC szamot a SMS oldalon. "
                      "Telekom Domino eseten a hivatalos uzenetkozpont-szam: +36309888000.";
    case 512: return "SIM nincs kesz.";
    case 514: return "Ervenytelen telefonszam vagy parameter.";
    case 515: return "Modem foglalt.";
    default: return "Nem ismert CMS kod.";
  }
}

String signalHint() {
  int csq = modem.getSignalQuality();
  gModem.signalQuality = csq;

  if(csq == 99)
    return "Jelerosseg ismeretlen (CSQ=99).";

  if(csq <= 0)
    return "Nincs hasznalhato GSM jel (CSQ=" + String(csq) + ").";

  if(csq < 7)
    return "Gyenge GSM jel (CSQ=" + String(csq) + "/31).";

  return "GSM jel: CSQ=" + String(csq) + "/31.";
}