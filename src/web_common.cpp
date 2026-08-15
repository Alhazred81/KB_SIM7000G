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

String htmlEscape(const String& in) {
  String out; out.reserve(in.length());
  for(size_t i=0;i<in.length();i++){
    char c = in[i];
    switch(c){
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '&':  out += "&amp;";  break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;";  break;
      default:
        if((uint8_t)c >= 0x20 && (uint8_t)c < 0x7F) out += c;
        break;
    }
  }
  return out;
}

String jsEscape(const String& in) {
  String out; out.reserve(in.length());
  for(size_t i=0;i<in.length();i++){
    char c = in[i];
    switch(c){
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': break;
      default:
        if((uint8_t)c >= 0x20 && (uint8_t)c < 0x7F) out += c;
        break;
    }
  }
  return out;
}

String satText(int value) {
  return value >= 0 ? String(value) : "n/a";
}

String ageText(unsigned long stamp) {
  if(stamp == 0) return "meg nem";
  unsigned long s = (millis() - stamp) / 1000UL;
  if(s < 60) return String(s) + " s";
  return String(s / 60) + " p " + String(s % 60) + " s";
}

String sigBar(int q) {
  if(q==99||q==0) return "<span style='color:var(--err)'>Nincs jel</span>";
  int pct = (q*100)/31;
  String col = (q>=15?"var(--ok)":q>=7?"var(--warn)":"var(--err)");
  String s = String("<span style='color:")+col+"'>"+String(q)+"/31 ("+String(pct)+"%)</span>";
  return s;
}

