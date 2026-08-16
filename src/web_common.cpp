//web_common.cpp

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

String phoneInputBlock(const String& btnId, const String& prefix) {
  String fmtId = prefix + "Fmt";
  String hiddenId = prefix + "Hidden";
  String hintId = prefix + "Hint";
  String h = "<label>Telefonszam</label>"
    "<div style='display:flex;gap:6px;align-items:center'>"
    "<span style='background:#0a0a18;border:1px solid var(--border);border-radius:10px;"
    "padding:10px 10px;font-size:14px;color:var(--txt2);white-space:nowrap'>+36</span>"
    "<input type='text' id='" + fmtId + "' inputmode='numeric' placeholder='30 123 4567' "
    "maxlength='12' oninput='fmtNum_" + prefix + "(this)' autocomplete='tel-national' style='flex:1'>"
    "</div>"
    "<input type='hidden' name='num' id='" + hiddenId + "'>"
    "<div class='hint' id='" + hintId + "'>Add meg a szamot ekezet es +36 nelkul, pl. 30 123 4567</div>";
  h += "<script>function fmtNum_" + prefix + "(el){"
    "var digits = el.value.replace(/\\D/g,'').substring(0,9);"
    "var out = '';"
    "if(digits.length>0) out += digits.substring(0,2);"
    "if(digits.length>2)  out += ' ' + digits.substring(2,5);"
    "if(digits.length>5)  out += ' ' + digits.substring(5,9);"
    "el.value = out;"
    "document.getElementById('" + hiddenId + "').value = '+36' + digits;"
    "var hint = document.getElementById('" + hintId + "');"
    "var btn  = document.getElementById('" + btnId + "');"
    "if(digits.length === 9){"
      "hint.style.color='var(--ok)';"
      "hint.innerText='+36 ' + out + ' - rendben';"
      "if(btn) btn.disabled = false;"
    "} else {"
      "hint.style.color='var(--txt3)';"
      "hint.innerText='Meg ' + (9-digits.length) + ' szamjegy hianyzik.';"
      "if(btn) btn.disabled = true;"
    "}"
  "}"
  "function prepNum_" + prefix + "(){"
    "var digits = document.getElementById('" + fmtId + "').value.replace(/\\D/g,'');"
    "return digits.length === 9;"
  "}</script>";
  return h;
}

String smartErrorBox(const String& err) {
  if(err.length() == 0) return "";

  String target = "";       
  String actionLabel = "";  

  if(err.indexOf("PIN") >= 0 || err.indexOf("SIM hiba") >= 0 ||
     err.indexOf("PUK") >= 0 || err.indexOf("nincs mentve") >= 0) {
    target = "/cfg";
    actionLabel = "Ugras a PIN beallitasahoz &#8250;";
  } else if(err.indexOf("halozat") >= 0 || err.indexOf("Halozat") >= 0 ||
            err.indexOf("antenna") >= 0 || err.indexOf("jel") >= 0) {
    target = "/diag";
    actionLabel = "Diagnosztika megnyitasa &#8250;";
  }

  String box = "<div class='msg err'";
  if(target.length()) box += " style='cursor:pointer' onclick=\"location.href='" + target + "'\"";
  box += ">";
  box += err;
  if(target.length()) {
    box += "<div style='margin-top:6px;font-weight:700;text-decoration:underline'>";
    box += actionLabel;
    box += "</div>";
  }
  box += "</div>";
  return box;
}

String compassAbbrev(float deg) {
  const char* dirs[] = {"E","EK","K","DK","D","DNy","Ny","ENy"};
  int idx = (int)((deg + 22.5f) / 45.0f) % 8;
  if(idx < 0) idx += 8;
  return String(dirs[idx]);
}

String sensStatusJsonEntry(const String& key, bool enabled, bool hasEverRead, bool ok, const String& value) {
  String j = "\"" + key + "\":{";
  j += "\"enabled\":" + String(enabled ? "true" : "false") + ",";
  j += "\"hasEverRead\":" + String(hasEverRead ? "true" : "false") + ",";
  j += "\"ok\":" + String(ok ? "true" : "false") + ",";
  j += "\"value\":\"" + jsEscape(value) + "\"";
  j += "}";
  return j;
}

String htmlHead(const String& title, const String& active) {
  String h = F("<!DOCTYPE html><html lang='hu'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='theme-color' content='#0d0d1a'>"
    "<meta name='apple-mobile-web-app-capable' content='yes'>"
    "<title>");
  h += title;
  h += F("</title>"
    "<link rel='stylesheet' href='/s.css'>"
    "</head><body>"
    "<nav>"
    "<a href='/' id='n1'>&#127968; F&#337;oldal</a>"
    "<a href='/comm' id='n2'>&#128172; Komm.</a>"
    "<a href='/gnss' id='n6'>&#128752; GPS</a>"
    "<a href='/sensors' id='n7'>&#127777; Szenzor</a>"
    "<a href='/expert' id='n8'>&#9889; Expert</a>"
    "<a href='/cfg' id='n4'>&#9881; Konfig</a>"
    "<a href='/diag' id='n5'>&#128202; Diag</a>"
    "</nav>"
    "<div class='wrap'>"
    "<script>document.getElementById('n");
  h += active;
  h += F("').className='on';</script>");
  return h;
}

String htmlFoot() {
  return "</div></body></html>";
}