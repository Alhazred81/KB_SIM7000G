//crypto.h

#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

String xteaEncryptStr(const String& plain);
String xteaDecryptStr(const uint8_t raw[8]);

void saveApPass(const String& pass);
String loadApPass();

void savePin(const String& pin);
String loadPin();
void clearPin();

void saveCCID(const String& ccid);
String loadCCID();
void clearCCID();

void xteaSaveBlock(int addr, int blockBytes, const String& plain);
String xteaLoadBlock(int addr, int blockBytes);

void saveStaCreds(const String& ssid, const String& pass);
String loadStaSSID();
String loadStaPass();
void clearStaCreds();