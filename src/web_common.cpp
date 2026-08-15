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

String normalizeAtCommand(String cmd) {
  cmd.trim();
  if(cmd.length() == 0) return "AT";
  cmd.toUpperCase();
  if(cmd == "AT") return "AT";
  if(cmd.startsWith("AT")) return cmd;
  if(cmd.startsWith("+")) return "AT" + cmd;
  return "AT" + cmd;
}

int base64DecodeChar(char c) {
  if(c >= 'A' && c <= 'Z') return c - 'A';
  if(c >= 'a' && c <= 'z') return c - 'a' + 26;
  if(c >= '0' && c <= '9') return c - '0' + 52;
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}

static const char B64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64Encode(const uint8_t* data, size_t len) {
  String out;
  out.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  while(i + 3 <= len) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i+1] << 8) | data[i+2];
    out += B64_CHARS[(n >> 18) & 0x3F];
    out += B64_CHARS[(n >> 12) & 0x3F];
    out += B64_CHARS[(n >> 6)  & 0x3F];
    out += B64_CHARS[n & 0x3F];
    i += 3;
  }
  size_t rem = len - i;
  if(rem == 1) {
    uint32_t n = (uint32_t)data[i] << 16;
    out += B64_CHARS[(n >> 18) & 0x3F];
    out += B64_CHARS[(n >> 12) & 0x3F];
    out += "==";
  } else if(rem == 2) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i+1] << 8);
    out += B64_CHARS[(n >> 18) & 0x3F];
    out += B64_CHARS[(n >> 12) & 0x3F];
    out += B64_CHARS[(n >> 6)  & 0x3F];
    out += "=";
  }
  return out;
}

size_t base64Decode(const String& in, uint8_t* buf, size_t maxLen) {
  size_t outLen = 0;
  int vals[4]; int vi = 0;
  for(size_t i = 0; i < in.length() && outLen < maxLen; i++) {
    char c = in[i];
    if(c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
    int v = base64DecodeChar(c);
    if(v < 0) continue;
    vals[vi++] = v;
    if(vi == 4) {
      uint32_t n = ((uint32_t)vals[0] << 18) | ((uint32_t)vals[1] << 12) | ((uint32_t)vals[2] << 6) | vals[3];
      if(outLen < maxLen) buf[outLen++] = (n >> 16) & 0xFF;
      if(outLen < maxLen) buf[outLen++] = (n >> 8) & 0xFF;
      if(outLen < maxLen) buf[outLen++] = n & 0xFF;
      vi = 0;
    }
  }
  if(vi >= 2) {
    uint32_t n = ((uint32_t)vals[0] << 18) | ((uint32_t)vals[1] << 12);
    if(vi >= 2 && outLen < maxLen) buf[outLen++] = (n >> 16) & 0xFF;
    if(vi >= 3) {
      n |= (uint32_t)vals[2] << 6;
      if(outLen < maxLen) buf[outLen++] = (n >> 8) & 0xFF;
    }
  }
  return outLen;
}