#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// ─── XTEA (64-bit blokk, 128-bit kulcs, 32 kör) ────────────
static void xteaEncrypt(uint32_t v[2], const uint32_t k[4]) {
  uint32_t v0=v[0], v1=v[1], sum=0;
  const uint32_t delta=0x9E3779B9;
  for(int i=0;i<32;i++){
    v0 += (((v1<<4)^(v1>>5))+v1) ^ (sum+k[sum&3]);
    sum += delta;
    v1 += (((v0<<4)^(v0>>5))+v0) ^ (sum+k[(sum>>11)&3]);
  }
  v[0]=v0; v[1]=v1;
}

static void xteaDecrypt(uint32_t v[2], const uint32_t k[4]) {
  uint32_t v0=v[0], v1=v[1];
  const uint32_t delta=0x9E3779B9;
  uint32_t sum=delta*32;
  for(int i=0;i<32;i++){
    v1 -= (((v0<<4)^(v0>>5))+v0) ^ (sum+k[(sum>>11)&3]);
    sum -= delta;
    v0 -= (((v1<<4)^(v1>>5))+v1) ^ (sum+k[sum&3]);
  }
  v[0]=v0; v[1]=v1;
}

// Kulcs az ESP32 egyedi chip-ID-jából (eszközfüggő)
static void buildKey(uint32_t k[4]) {
  uint64_t id = ESP.getEfuseMac();
  k[0] = (uint32_t)(id & 0xFFFFFFFF) ^ 0xDEADBEEF;
  k[1] = (uint32_t)(id >> 32)        ^ 0xCAFEBABE;
  k[2] = k[0] ^ 0x13572468;
  k[3] = k[1] ^ 0x86421357;
}

// ─── XTEA-vel titkosít/visszafejt egy max 8 byte-os stringet ─
String xteaEncryptStr(const String& plain) {
  uint32_t k[4]; buildKey(k);
  uint8_t buf[8] = {0};
  int len = min((int)plain.length(), 8);
  for(int i=0;i<len;i++) buf[i] = (uint8_t)plain[i];
  uint32_t v[2]; memcpy(v, buf, 8);
  xteaEncrypt(v, k);
  memcpy(buf, v, 8);
  // hex string visszatérítés
  String out = "";
  for(int i=0;i<8;i++){
    if(buf[i]<16) out+="0";
    out += String(buf[i], HEX);
  }
  return out;
}

String xteaDecryptStr(const uint8_t raw[8]) {
  uint32_t k[4]; buildKey(k);
  uint8_t buf[8];
  memcpy(buf, raw, 8);
  uint32_t v[2]; memcpy(v, buf, 8);
  xteaDecrypt(v, k);
  memcpy(buf, v, 8);
  String out = "";
  for(int i=0;i<8;i++){
    if(buf[i]==0) break;
    if(buf[i]<0x20||buf[i]>0x7E) return ""; // nem printable → szemét
    out += (char)buf[i];
  }
  return out;
}

// ─── AP jelszó: 32 byte = 4 XTEA blokk (max 31 char + null) ─
void saveApPass(const String& pass) {
  uint32_t k[4]; buildKey(k);
  uint8_t buf[32] = {0};
  int len = min((int)pass.length(), 31);
  for(int i=0;i<len;i++) buf[i] = (uint8_t)pass[i];
  // 4 blokk titkosítás
  for(int b=0;b<4;b++){
    uint32_t v[2]; memcpy(v, buf+b*8, 8);
    xteaEncrypt(v, k);
    memcpy(buf+b*8, v, 8);
  }
  for(int i=0;i<32;i++) EEPROM.write(ADDR_AP_PASS+i, buf[i]);
  EEPROM.commit();
}

String loadApPass() {
  uint32_t k[4]; buildKey(k);
  uint8_t buf[32];
  for(int i=0;i<32;i++) buf[i] = EEPROM.read(ADDR_AP_PASS+i);
  // Inicializálatlan?
  bool allFF=true;
  for(int i=0;i<32;i++) if(buf[i]!=0xFF){allFF=false;break;}
  if(allFF) return DEFAULT_AP_PASS;
  // 4 blokk visszafejtés
  for(int b=0;b<4;b++){
    uint32_t v[2]; memcpy(v, buf+b*8, 8);
    xteaDecrypt(v, k);
    memcpy(buf+b*8, v, 8);
  }
  String out = "";
  for(int i=0;i<31;i++){
    if(buf[i]==0) break;
    if(buf[i]<0x20||buf[i]>0x7E) return DEFAULT_AP_PASS;
    out += (char)buf[i];
  }
  return (out.length()>=8) ? out : DEFAULT_AP_PASS;
}

// ─── SIM PIN ────────────────────────────────────────────────
void savePin(const String& pin) {
  uint32_t k[4]; buildKey(k);
  uint8_t buf[8] = {0};
  int len = min((int)pin.length(), 8);
  for(int i=0;i<len;i++) buf[i] = (uint8_t)pin[i];
  uint32_t v[2]; memcpy(v, buf, 8);
  xteaEncrypt(v, k);
  memcpy(buf, v, 8);
  for(int i=0;i<8;i++) EEPROM.write(ADDR_PIN+i, buf[i]);
  EEPROM.write(ADDR_PIN_FLAG, MAGIC_BYTE);
  EEPROM.commit();
  Serial.println(F("[CRYPTO] PIN elmentve (titkositva)."));
}

String loadPin() {
  if(EEPROM.read(ADDR_PIN_FLAG) != MAGIC_BYTE) return "";
  uint8_t buf[8];
  for(int i=0;i<8;i++) buf[i] = EEPROM.read(ADDR_PIN+i);
  bool allZ=true, allF=true;
  for(int i=0;i<8;i++){if(buf[i]!=0x00)allZ=false; if(buf[i]!=0xFF)allF=false;}
  if(allZ||allF) return "";
  String pin = xteaDecryptStr(buf);
  // Validáció: 4-8 ASCII szám
  if(pin.length()<4) return "";
  for(char c:pin) if(c<'0'||c>'9') return "";
  return pin;
}

void clearPin() {
  for(int i=0;i<8;i++) EEPROM.write(ADDR_PIN+i, 0x00);
  EEPROM.write(ADDR_PIN_FLAG, 0x00);
  EEPROM.commit();
  Serial.println(F("[CRYPTO] PIN torolve."));
}

// ─── CCID ───────────────────────────────────────────────────
void saveCCID(const String& ccid) {
  int len = min((int)ccid.length(), 19);
  for(int i=0;i<20;i++) EEPROM.write(ADDR_CCID+i, i<len ? (uint8_t)ccid[i] : 0);
  EEPROM.write(ADDR_CCID_FLAG, MAGIC_BYTE);
  EEPROM.commit();
  Serial.println("[CRYPTO] CCID mentve: "+ccid);
}

String loadCCID() {
  if(EEPROM.read(ADDR_CCID_FLAG) != MAGIC_BYTE) return "";
  String out = "";
  for(int i=0;i<20;i++){
    char c=(char)EEPROM.read(ADDR_CCID+i);
    if(c==0) break;
    out+=c;
  }
  return out;
}

void clearCCID() {
  for(int i=0;i<20;i++) EEPROM.write(ADDR_CCID+i, 0);
  EEPROM.write(ADDR_CCID_FLAG, 0x00);
  EEPROM.commit();
}

// ─── Általános, tetszőleges hosszú string XTEA mentés/betöltés ─
// blockBytes-nek 8-cal oszthatónak kell lennie (XTEA 64-bit blokk).
// A tárolt szöveg mindig null-terminált marad a puffer belsejében.
void xteaSaveBlock(int addr, int blockBytes, const String& plain) {
  uint32_t k[4]; buildKey(k);
  uint8_t* buf = (uint8_t*)calloc(blockBytes, 1);
  if(!buf) return; // OOM védelem - nagyon valószínűtlen ekkora bufferre
  int len = min((int)plain.length(), blockBytes - 1);
  for(int i=0;i<len;i++) buf[i] = (uint8_t)plain[i];
  int blocks = blockBytes / 8;
  for(int b=0;b<blocks;b++){
    uint32_t v[2]; memcpy(v, buf+b*8, 8);
    xteaEncrypt(v, k);
    memcpy(buf+b*8, v, 8);
  }
  for(int i=0;i<blockBytes;i++) EEPROM.write(addr+i, buf[i]);
  EEPROM.commit();
  free(buf);
}

String xteaLoadBlock(int addr, int blockBytes) {
  uint32_t k[4]; buildKey(k);
  uint8_t* buf = (uint8_t*)malloc(blockBytes);
  if(!buf) return "";
  for(int i=0;i<blockBytes;i++) buf[i] = EEPROM.read(addr+i);

  bool allFF = true;
  for(int i=0;i<blockBytes;i++) if(buf[i]!=0xFF){allFF=false;break;}
  if(allFF){ free(buf); return ""; }

  int blocks = blockBytes / 8;
  for(int b=0;b<blocks;b++){
    uint32_t v[2]; memcpy(v, buf+b*8, 8);
    xteaDecrypt(v, k);
    memcpy(buf+b*8, v, 8);
  }
  String out = "";
  for(int i=0;i<blockBytes-1;i++){
    if(buf[i]==0) break;
    if(buf[i]<0x20 || buf[i]>0x7E){ free(buf); return ""; } // szemét → üres
    out += (char)buf[i];
  }
  free(buf);
  return out;
}

// ─── STA (kliens) WiFi hitelesítő adatok ────────────────────
void saveStaCreds(const String& ssid, const String& pass) {
  xteaSaveBlock(ADDR_STA_SSID, 96, ssid);
  xteaSaveBlock(ADDR_STA_PASS, 64, pass);
  EEPROM.write(ADDR_STA_FLAG, MAGIC_BYTE);
  EEPROM.commit();
  Serial.println("[CRYPTO] STA WiFi adatok mentve: " + ssid);
}

String loadStaSSID() {
  if(EEPROM.read(ADDR_STA_FLAG) != MAGIC_BYTE) return "";
  return xteaLoadBlock(ADDR_STA_SSID, 96);
}

String loadStaPass() {
  if(EEPROM.read(ADDR_STA_FLAG) != MAGIC_BYTE) return "";
  return xteaLoadBlock(ADDR_STA_PASS, 64);
}

void clearStaCreds() {
  for(int i=0;i<96;i++) EEPROM.write(ADDR_STA_SSID+i, 0);
  for(int i=0;i<64;i++) EEPROM.write(ADDR_STA_PASS+i, 0);
  EEPROM.write(ADDR_STA_FLAG, 0x00);
  EEPROM.commit();
  Serial.println(F("[CRYPTO] STA WiFi adatok torolve."));
}
