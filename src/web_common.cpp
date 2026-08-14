#include <Arduino.h>
#include "web_common.h"

#define DIAG_MAX 20

static String diagLog[DIAG_MAX];
static int diagHead = 0;
static int diagCount = 0;

void diagAdd(const String& line) {
  Serial.println("[DIAG] " + line);
  diagLog[diagHead] = line;
  diagHead = (diagHead + 1) % DIAG_MAX;
  if(diagCount < DIAG_MAX) diagCount++;
}

String diagDump() {
  String out = "";
  int start = (diagCount < DIAG_MAX) ? 0 : diagHead;

  for(int i = 0; i < diagCount; i++) {
    out += diagLog[(start + i) % DIAG_MAX] + "\n";
  }

  return out;
}