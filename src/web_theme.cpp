//web_theme.cpp

#include "web_theme.h"

const char CSS[] PROGMEM = R"css(
*{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#0d0d1a;--card:#161628;--border:#1e2a4a;
  --accent:#00c8ff;--accent2:#0077aa;
  --ok:#27ae60;--err:#e74c3c;--warn:#f39c12;
  --txt:#e8eaf0;--txt2:#8899bb;--txt3:#556080
}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
  background:var(--bg);color:var(--txt);min-height:100vh;font-size:15px}
nav{display:flex;background:var(--card);border-bottom:1px solid var(--border);
  position:sticky;top:0;z-index:99}
nav a{flex:1;text-align:center;padding:10px 1px;color:var(--txt2);
  text-decoration:none;font-size:9.5px;font-weight:600;letter-spacing:.2px;
  border-bottom:2px solid transparent;transition:.15s;white-space:nowrap}
nav a.on{color:var(--accent);border-bottom-color:var(--accent)}
.wrap{padding:14px;max-width:1180px;margin:0 auto;display:grid;grid-template-columns:1fr;gap:14px;align-items:start}
h1{font-size:17px;font-weight:700;color:var(--accent);text-align:center;
  margin-bottom:0;letter-spacing:.5px;grid-column:1/-1}
.card{background:var(--card);border:1px solid var(--border);border-radius:14px;
  padding:14px;margin-bottom:0;min-width:0}
.card h2{font-size:12px;font-weight:700;color:var(--accent);
  text-transform:uppercase;letter-spacing:.8px;margin-bottom:10px}
label{display:block;font-size:12px;color:var(--txt2);margin-bottom:4px;margin-top:10px}
label:first-of-type{margin-top:0}
input[type=text],input[type=password],textarea{
  width:100%;padding:10px 12px;border-radius:10px;
  border:1px solid var(--border);background:#0a0a18;
  color:var(--txt);font-size:14px;outline:none;
  -webkit-appearance:none;transition:.15s}
input:focus,textarea:focus{border-color:var(--accent)}
input,textarea,button,.row .v{overflow-wrap:anywhere}
textarea{min-height:80px;resize:none;font-family:inherit}
.counter{font-size:11px;color:var(--txt3);text-align:right;margin-top:3px}
button{width:100%;padding:12px;border:none;border-radius:10px;
  background:var(--accent);color:#000;font-weight:700;font-size:14px;
  cursor:pointer;margin-top:10px;-webkit-tap-highlight-color:transparent;transition:.15s}
button:active{opacity:.8}
button:disabled{opacity:.35;cursor:not-allowed}
button.sec{background:#1e2a4a;color:var(--accent);border:1px solid var(--accent)}
button.danger{background:var(--err);color:#fff}
button.warn{background:var(--warn);color:#000}
.msg{padding:10px 12px;border-radius:10px;font-size:13px;
  margin-bottom:12px;text-align:center;font-weight:500}
.msg.ok{background:#0d2818;color:var(--ok);border:1px solid var(--ok)}
.msg.err{background:#2a0d0d;color:var(--err);border:1px solid var(--err)}
.msg.warn{background:#2a1e0d;color:var(--warn);border:1px solid var(--warn)}
.row{display:flex;justify-content:space-between;align-items:center;
  padding:6px 0;border-bottom:1px solid var(--border)}
.row:last-child{border-bottom:none}
.row .k{font-size:12px;color:var(--txt2)}
.row .v{font-size:13px;color:var(--txt);font-weight:600;text-align:right}
.row .v.g{color:var(--ok)}
.row .v.r{color:var(--err)}
.row .v.y{color:var(--warn)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:5px}
.dot.g{background:var(--ok)}.dot.r{background:var(--err)}.dot.y{background:var(--warn)}
.diag{font-family:'Courier New',monospace;font-size:11px;color:#7aff7a;
  background:#000;border-radius:8px;padding:10px;max-height:420px;
  overflow-y:auto;white-space:pre-wrap;overflow-wrap:anywhere}
.hint{font-size:11px;color:var(--txt3);margin-top:5px;line-height:1.4}
details.card{padding:16px}
details.card summary{list-style:none;outline:none}
details.card summary::-webkit-details-marker{display:none}
details.card summary::before{content:'\25B8';display:inline-block;margin-right:6px;
  transition:transform .15s;color:var(--accent)}
details.card[open] summary::before{transform:rotate(90deg)}

.sens-row{display:flex;align-items:center;gap:10px;padding:9px 0;
  border-bottom:1px solid var(--border)}
.sens-row:last-child{border-bottom:none}
.sens-toggle{position:relative;width:42px;height:24px;flex-shrink:0;cursor:pointer}
.sens-toggle input{opacity:0;width:0;height:0}
.sens-toggle .slider{position:absolute;inset:0;background:#3a3a4a;
  border-radius:24px;transition:.2s}
.sens-toggle .slider::before{content:'';position:absolute;width:18px;height:18px;
  left:3px;top:3px;background:#eee;border-radius:50%;transition:.2s}
.sens-toggle input:checked + .slider{background:var(--sens-color,var(--ok))}
.sens-toggle input:checked + .slider::before{transform:translateX(18px)}
.sens-name{flex:0 0 auto;min-width:92px;font-size:13px;color:var(--txt)}
.sens-value{flex:1;text-align:right;font-size:14px;font-weight:700;color:var(--txt)}
.sens-value.dim{color:var(--txt3);font-weight:400;font-size:12px}
.sens-dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:4px}
.sens-dot.g{background:var(--ok)}.sens-dot.y{background:var(--warn)}.sens-dot.r{background:var(--err)}.sens-dot.gray{background:#555}
.cb-row{display:flex;align-items:center;gap:8px;margin-top:8px}
.cb-row input[type=checkbox]{width:18px;height:18px;accent-color:var(--accent)}
.cb-row label{margin:0;font-size:13px;color:var(--txt);cursor:pointer}
select{width:100%;padding:10px 12px;border-radius:10px;border:1px solid var(--border);
  background:#0a0a18;color:var(--txt);font-size:14px;outline:none;
  -webkit-appearance:none}
.wrap>.msg,.wrap>form,.wrap>.hint{grid-column:1/-1}
@media (min-width:700px){
  body{font-size:16px}
  nav{justify-content:center;gap:6px;padding:0 14px}
  nav a{flex:0 1 150px;font-size:13px;padding:14px 10px}
  .wrap{grid-template-columns:repeat(2,minmax(0,1fr));padding:22px}
  .card.wide{grid-column:1/-1}
}
@media (min-width:1050px){
  .wrap{max-width:1320px;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));padding:26px}
  .card.wide{grid-column:span 2}
  .card.diag-card{grid-column:span 2}
  .card.full{grid-column:1/-1}
}
@media (min-width:1500px){
  .wrap{max-width:1560px;grid-template-columns:repeat(auto-fit,minmax(340px,1fr))}
  .card.wide{grid-column:span 1}
  .card.diag-card{grid-column:span 2}
  .card.full{grid-column:1/-1}
}
.netitem{display:flex;justify-content:space-between;align-items:center;
  padding:10px 12px;background:#0a0a18;border:1px solid var(--border);
  border-radius:10px;margin-bottom:6px;cursor:pointer;transition:.15s;gap:12px}
.netitem:active{background:#141428}
.netitem.picked{border-color:var(--accent)}
.netname{font-size:13px;color:var(--txt);overflow-wrap:anywhere}
.netmeta{font-size:11px;color:var(--txt2);white-space:nowrap}
)css";

extern WebServer server;

void handleCss() {
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.sendHeader("Content-Type", "text/css");
  server.send_P(200, "text/css", CSS);
}