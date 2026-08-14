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