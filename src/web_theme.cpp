#include "web_theme.h"
#include "web_common.h"
#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;
extern String macSuffix();

// Behúzzuk a globális változót a módváltáshoz
extern bool gFieldMode;

// ─── HTML Fejléc és Navigációs menü ───────────────────────────
String htmlHead(const String& title, const String& activeTab) {
  String s = "<!DOCTYPE html><html lang='hu'><head><meta charset='utf-8'>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  s += "<title>" + title + "</title>";
  
  // Íme az on-the-fly méhecske favicon! (Nincs szükség külön fájlra)
  s += "<link rel='icon' href=\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🐝</text></svg>\">";
  
  s += "<link rel='stylesheet' href='/s.css'>";
  s += "</head><body>";
  s += "<nav>";
  
  // ─── Alapvető fülek (Terep módban és Setup módban is látszanak) ───
  s += "<a href='/'" + String(activeTab=="1"?" class='on'":"") + ">🏠 Főoldal</a>";
  s += "<a href='/hives'" + String(activeTab=="9"?" class='on'":"") + ">🗺 Kaptárak</a>";
  s += "<a href='/hive'" + String(activeTab=="11"?" class='on'":"") + ">🐝 Kaptár</a>";

  // ─── Haladó fülek (Csak Setup módban látszanak) ───
  if (!gFieldMode) {
    s += "<a href='/gsm'" + String(activeTab=="2"?" class='on'":"") + ">📱 GSM</a>";
    s += "<a href='/iot'" + String(activeTab=="3"?" class='on'":"") + ">🌐 IoT</a>";
    s += "<a href='/gnss'" + String(activeTab=="6"?" class='on'":"") + ">🛰 GNSS</a>";
    s += "<a href='/sensors'" + String(activeTab=="7"?" class='on'":"") + ">🌡 Szenzor</a>";
    s += "<a href='/cfg'" + String(activeTab=="4"?" class='on'":"") + ">⚙ Konfig</a>";
    s += "<a href='/expert'" + String(activeTab=="8"?" class='on'":"") + ">⚠️ Expert</a>";
    s += "<a href='/diag'" + String(activeTab=="5"?" class='on'":"") + ">🩺 Diag</a>";
  }

  s += "</nav><div class='wrap'>";
  return s;
}

// ─── HTML Lábléc ──────────────────────────────────────────────
String htmlFoot() {
  return "</div></body></html>";
}

// ─── CSS Stíluslap (C++ nézetekhez) ───────────────────────────
void handleCss() {
  String css = R"css(
    :root{--bg:#05050a;--nav:#0d0d1a;--card:#141428;--txt:#e0e0e0;--txt2:#888;
    --border:#2a2a40;--accent:#4d4dff;--ok:#00cc66;--warn:#ff9900;--err:#ff3333}
    * {box-sizing:border-box;margin:0;padding:0;font-family:system-ui,-apple-system,sans-serif}
    body {background:var(--bg);color:var(--txt);font-size:14px;line-height:1.5}
    
    /* Navigáció */
    nav {background:var(--nav);display:flex;overflow-x:auto;border-bottom:1px solid var(--border);
    position:sticky;top:0;z-index:100;padding:0 8px;scrollbar-width:none}
    nav::-webkit-scrollbar{display:none}
    nav a {color:var(--txt2);text-decoration:none;padding:14px 16px;white-space:nowrap;
    font-weight:600;font-size:13px;border-bottom:2px solid transparent;transition:.2s}
    nav a:hover {color:var(--txt)}
    nav a.on {color:var(--accent);border-bottom-color:var(--accent)}
    
    /* Elrendezés (Szigorúan fix méretű Flexbox) */
    .wrap {max-width:1850px;margin:0 auto;padding:16px;display:flex;flex-wrap:wrap;gap:16px;align-items:flex-start}
    h1 {width:100%;font-size:20px;margin-bottom:-4px;color:#fff}
    .card {background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px;
    width:340px;max-width:100%;flex:0 0 auto}
    .card.wide {width:696px;max-width:100%;flex:0 0 auto}
    .card.full {width:100%;flex:0 0 auto}
    h2 {font-size:14px;text-transform:uppercase;letter-spacing:1px;color:var(--txt2);
    margin-bottom:12px;border-bottom:1px solid var(--border);padding-bottom:6px}
    
    /* Sorok */
    .row {display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid rgba(255,255,255,.03)}
    .row:last-child {border:none}
    .k {color:var(--txt2)}
    .v {font-weight:600;text-align:right}
    .g {color:var(--ok)} .y {color:var(--warn)} .r {color:var(--err)}
    
    /* Form elemek */
    input, select, textarea {width:100%;background:#0a0a18;color:#fff;border:1px solid var(--border);
    border-radius:8px;padding:10px;margin-bottom:12px;font-size:14px;outline:none}
    input:focus, select:focus, textarea:focus {border-color:var(--accent)}
    button {background:var(--accent);color:#fff;border:none;border-radius:8px;padding:10px 16px;
    font-size:14px;font-weight:600;cursor:pointer;width:100%;transition:.2s}
    button:hover {filter:brightness(1.1)}
    button:disabled {opacity:.5;cursor:not-allowed}
    button.sec {background:transparent;border:1px solid var(--border);color:var(--txt)}
    button.sec:hover {background:var(--border)}
    button.warn {background:var(--warn);color:#000}
    button.danger {background:var(--err);color:#fff}
    label {display:block;font-size:12px;color:var(--txt2);margin-bottom:4px;margin-top:4px}
    
    /* Üzenetek */
    .msg {padding:10px;border-radius:8px;margin-bottom:12px;font-size:13px;border-left:4px solid}
    .msg.ok {background:rgba(0,204,102,.1);border-color:var(--ok);color:var(--ok)}
    .msg.err {background:rgba(255,51,51,.1);border-color:var(--err);color:var(--err)}
    .msg.warn {background:rgba(255,153,0,.1);border-color:var(--warn);color:var(--warn)}
    .hint {font-size:11px;color:var(--txt2);margin-bottom:12px}
    
    /* Diag doboz */
    .diag {font-family:monospace;font-size:11px;background:#05050a;padding:10px;
    border-radius:8px;overflow-x:auto;white-space:pre-wrap;color:#aaa}
    
    /* Checkbox / Szenzor sorok */
    .cb-row {display:flex;align-items:center;gap:8px;margin-bottom:12px}
    .cb-row input {width:auto;margin:0}
    .cb-row label {margin:0;font-size:14px;color:var(--txt)}
    .sens-row {display:flex;align-items:center;padding:8px 0;border-bottom:1px solid var(--border);gap:12px}
    .sens-row:last-child {border:none}
    .sens-name {flex:1;font-weight:600}
    .sens-value {font-family:monospace;font-size:14px;white-space:nowrap;text-align:right}
    .sens-value.dim {color:var(--txt2);font-size:12px}
    .sens-toggle {position:relative;display:inline-block;width:40px;height:22px;flex-shrink:0}
    .sens-toggle input {opacity:0;width:0;height:0}
    .slider {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;
    background-color:var(--border);transition:.3s;border-radius:22px}
    .slider:before {position:absolute;content:"";height:16px;width:16px;left:3px;bottom:3px;
    background-color:#aaa;transition:.3s;border-radius:50%}
    input:checked + .slider {background-color:var(--sens-color, var(--ok))}
    input:checked + .slider:before {transform:translateX(18px);background-color:#fff}
    
    /* Extra infó buborék */
    .pin-info {display:inline-block;margin-left:6px;color:var(--accent);cursor:pointer;
    font-size:12px;position:relative}
    .pin-bubble {display:none;position:absolute;left:100%;top:50%;transform:translateY(-50%);
    background:var(--nav);border:1px solid var(--border);padding:4px 8px;border-radius:4px;
    white-space:nowrap;z-index:10;margin-left:8px;color:var(--txt)}
    .pin-info.open .pin-bubble {display:block}
  )css";
  server.send(200, "text/css", css);
}