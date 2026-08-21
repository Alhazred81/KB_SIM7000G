#include "calendar.h"

String getMoonPhaseInfo(int year, int month, int day) {
  return "Holdfázis adatok";
}

String getBeekeepingCalendar() {
  return "Méhészeti teendők";
}

String getCalendarCardHtml() {
  String html = "<div class='card'><h2>Méhészeti Naptár</h2>";
  html += "<p class='hint'>Naptár modul betöltése folyamatban...</p>";
  html += "</div>";
  return html;
}