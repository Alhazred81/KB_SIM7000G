//calendar.h

#ifndef CALENDAR_H
#define CALENDAR_H

#include <Arduino.h>

String getMoonPhaseInfo(int year, int month, int day);
String getBeekeepingCalendar();
String getCalendarCardHtml();

#endif