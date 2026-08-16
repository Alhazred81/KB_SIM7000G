//modem_utils.h

#pragma once

#include <Arduino.h>

void modemDrain(unsigned long ms = 80);
String modemReadUntilFinal(unsigned long timeoutMs, bool stopAtPrompt = false);

int parseAtErrorCode(const String& resp, const String& token);

String cmsErrorText(int code);
String signalHint();
