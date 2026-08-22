#include "calendar.h"
#include <Arduino.h>

// Példa a naptár generálására (ma + 7 nap)
String getCalendarCardHtml() {
  String html = "<div class='card'><h2>Méhészeti Naptár</h2>";
  html += "<div style='font-size:13px; color:var(--txt2); margin-bottom:10px;'>Aktuális teendők (Ma + 7 nap)</div>";
  
  html += "<div style='display:flex; flex-direction:column; gap:8px;'>";

  // Itt a lényeg: a ciklus 0-tól 7-ig megy (azaz ma + 7 nap = 8 sor)
  for (int i = 0; i < 8; i++) {
    // Itt számolhatod ki a valós dátumot az NTP / Rtc idő alapján
    html += "<div style='display:flex; justify-content:space-between; padding:6px 8px; background:rgba(255,255,255,0.02); border-radius:6px;'>";
    html += "<span style='font-weight:600;'>Ma +" + String(i) + " nap</span>";
    
    // Példa sablon logika (pl. Anyanevelési Mesterterv / etetés napjai)
    if (i == 0) html += "<span style='color:var(--ok);'>Etetés / Ellenőrzés</span>";
    else if (i == 3) html += "<span style='color:var(--warn);'>Átlarcozás</span>";
    else html += "<span style='color:var(--txt3);'>Nyugalmi nap</span>";
    
    html += "</div>";
  }

  html += "</div></div>";
  return html;
}