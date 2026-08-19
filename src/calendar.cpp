//calendar.cpp

#include "calendar.h"
#include <cmath>

String getMoonPhaseInfo(int year, int month, int day) {
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    long jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    
    double daysSinceNew = jdn - 2451550.1;
    double synodicMonth = 29.53058867;
    double phase = fmod(daysSinceNew, synodicMonth);
    if (phase < 0) phase += synodicMonth;

    int phaseIndex = (int)((phase / synodicMonth) * 8 + 0.5) % 8;

    const char* phases[] = {
        "Újhold", "Növekvő sarló", "Első negyed", "Növekvő domború",
        "Telihold", "Fogyó domború", "Utolsó negyed", "Fogyó sarló"
    };
    const char* icons[] = { "🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘" };
    
    return String(icons[phaseIndex]) + " " + String(phases[phaseIndex]);
}

String getBeekeepingCalendar() {
    // Itt a dinamikus naptár logika (például a "Petés Anyanevelési Mesterterv" dátumai)
    String html = "<div class='card' style='margin-bottom:15px;'>";
    html += "<h2>📅 Méhészeti Naptár</h2>";
    html += "<ul style='padding-left:20px; font-size:0.9rem;'>";
    html += "<li><b>Augusztus 19.:</b> Téli felkészítés, atkakezelés.</li>";
    html += "<li><b>Szeptember:</b> Serkentő etetés.</li>";
    html += "</ul></div>";
    return html;
}

String getCalendarCardHtml() {
    // Aktuális dátum: 2026. augusztus 19.
    String moonInfo = getMoonPhaseInfo(2026, 8, 19);

    String html = "<div class='card sun-moon-card' style='background: linear-gradient(135deg, #1e1b4b, #311026); border-radius: 12px; padding: 16px; color: #fff; text-align: center; box-shadow: 0 4px 6px rgba(0,0,0,0.3); margin-bottom: 15px;'>";
    html += "<h2 style='margin-top:0; font-size:1.1rem; color:#f472b6;'>🌅 Természet & Naptár</h2>";
    
    // SVG Naplemente
    html += "<div style='width:100%; max-width:280px; margin:0 auto;'><svg viewBox='0 0 300 120' style='width:100%; height:auto; border-radius:8px;'>";
    html += "<defs><linearGradient id='skyGrad' x1='0%' y1='0%' x2='0%' y2='100%'><stop offset='0%' stop-color='#4c1d95'/><stop offset='50%' stop-color='#9a3412'/><stop offset='100%' stop-color='#ea580c'/></linearGradient></defs>";
    html += "<rect width='300' height='120' fill='url(#skyGrad)'/><circle cx='150' cy='70' r='22' fill='#fde047' opacity='0.9'/>";
    html += "<path d='M0 100 Q 100 60 300 90 L 300 120 L 0 120 Z' fill='#7c2d12' opacity='0.7'/>";
    html += "<path d='M0 110 Q 180 75 300 105 L 300 120 L 0 120 Z' fill='#431407'/>";
    html += "<rect x='145' y='92' width='10' height='8' rx='1' fill='#18181b'/>";
    html += "<rect x='143' y='90' width='14' height='2' fill='#18181b'/></svg></div>";

    html += "<div style='display:flex; justify-content:space-around; margin-top:12px; font-size:0.9rem;'>";
    html += "<div><p style='margin:0; color:#cbd5e1;'>Napnyugta</p><p style='margin:4px 0 0; font-weight:bold;'>19:35</p></div>";
    html += "<div><p style='margin:0; color:#cbd5e1;'>Holdfázis</p><p style='margin:4px 0 0; font-weight:bold;'>" + moonInfo + "</p></div>";
    html += "</div></div>";
    
    html += getBeekeepingCalendar(); // Ide hívjuk be a naptár nézetet
    
    return html;
}