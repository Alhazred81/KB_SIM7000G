#pragma once

#include <Arduino.h>

void diagAdd(const String& line);
String diagDump();

String htmlHead(const String& title, const String& active);
String htmlFoot();


String satText(int value);
String satRow(const String& systemName, int count);

String ageText(unsigned long stamp);

String htmlEscape(const String& in);
String jsEscape(const String& in);

String sigBar(int q);

String modemBusyReason();
bool sendModemBusyPage(const String& title,
                       const String& active,
                       const String& backUrl);

String normalizeAtCommand(String cmd);
String smartErrorBox(const String& err);
String htmlEscape(const String& in);
String jsEscape(const String& in);
String satText(int value);
String ageText(unsigned long stamp);

String modemBusyReason();

bool sendModemBusyPage(const String& title,
                       const String& active,
                       const String& backUrl);

String normalizeAtCommand(String cmd);

int base64DecodeChar(char c);

String base64Encode(const uint8_t* data, size_t len);

size_t base64Decode(const String& in, uint8_t* buf, size_t maxLen);