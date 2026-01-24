#include "ICalParser.h"
#include <stdarg.h>

ICalParser::ICalParser()
    : _debugCallback(nullptr), _parseTimeout(30000), _dataTimeout(5000) {}

void ICalParser::setDebugCallback(ICalDebugCallback callback) {
  _debugCallback = callback;
}

void ICalParser::setParseTimeout(unsigned long timeout) {
  _parseTimeout = timeout;
}

void ICalParser::setDataTimeout(unsigned long timeout) {
  _dataTimeout = timeout;
}

void ICalParser::debug(const char* msg) {
  if (_debugCallback) {
    _debugCallback(msg);
  }
}

void ICalParser::debugf(const char* fmt, ...) {
  if (_debugCallback) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _debugCallback(buf);
  }
}

// Check if date string ends with 'Z' (UTC indicator)
static bool isUtcTime(const char* dateStr) {
  size_t len = strlen(dateStr);
  return len > 0 && dateStr[len - 1] == 'Z';
}

// Convert UTC hour to US Eastern time (EST/EDT)
// Returns adjusted hour (may be negative or >= 24, caller handles day rollover)
static int utcToEasternHour(int year, int month, int day, int hour) {
  // Simplified DST check for US Eastern:
  // DST starts 2nd Sunday of March, ends 1st Sunday of November
  bool isDst = false;

  if (month > 3 && month < 11) {
    isDst = true;
  } else if (month == 3) {
    // 2nd Sunday of March: find it
    int firstDay = (1 + (year + year/4 - year/100 + year/400 + 13) % 7) % 7; // day of week for March 1
    int secondSunday = (firstDay == 0) ? 8 : (15 - firstDay);
    isDst = (day > secondSunday) || (day == secondSunday && hour >= 7);
  } else if (month == 11) {
    // 1st Sunday of November
    int firstDay = (1 + (year + year/4 - year/100 + year/400 + 13 + 31 + 30 + 31 + 31 + 30 + 31 + 30) % 7) % 7;
    int firstSunday = (firstDay == 0) ? 1 : (8 - firstDay);
    isDst = (day < firstSunday) || (day == firstSunday && hour < 6);
  }

  int offset = isDst ? -4 : -5;  // EDT = UTC-4, EST = UTC-5
  return hour + offset;
}

time_t ICalParser::parseDate(const char* dateStr) {
  struct tm tm = {0};
  int year, month, day, hour = 0, minute = 0, second = 0;

  if (strlen(dateStr) >= 8) {
    sscanf(dateStr, "%4d%2d%2d", &year, &month, &day);
    if (strlen(dateStr) >= 15 && dateStr[8] == 'T') {
      sscanf(dateStr + 9, "%2d%2d%2d", &hour, &minute, &second);
    }

    // Convert UTC to Eastern time if 'Z' suffix present
    if (isUtcTime(dateStr)) {
      int localHour = utcToEasternHour(year, month, day, hour);

      // Handle day rollover
      if (localHour < 0) {
        localHour += 24;
        day--;
        if (day == 0) {
          month--;
          if (month == 0) {
            month = 12;
            year--;
          }
          // Days in previous month
          static const int daysInMonth[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
          day = daysInMonth[month];
          if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
            day = 29;
          }
        }
      } else if (localHour >= 24) {
        localHour -= 24;
        day++;
        static const int daysInMonth[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        int maxDay = daysInMonth[month];
        if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
          maxDay = 29;
        }
        if (day > maxDay) {
          day = 1;
          month++;
          if (month > 12) {
            month = 1;
            year++;
          }
        }
      }
      hour = localHour;
    }

    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    return mktime(&tm);
  }
  return 0;
}

ICalRRule ICalParser::parseRRule(const char* rruleStr) {
  ICalRRule rule = {ICAL_FREQ_NONE, 1, 0, 0, 0};

  if (!rruleStr || strlen(rruleStr) == 0) {
    return rule;
  }

  if (strstr(rruleStr, "FREQ=DAILY"))
    rule.freq = ICAL_FREQ_DAILY;
  else if (strstr(rruleStr, "FREQ=WEEKLY"))
    rule.freq = ICAL_FREQ_WEEKLY;
  else if (strstr(rruleStr, "FREQ=MONTHLY"))
    rule.freq = ICAL_FREQ_MONTHLY;
  else if (strstr(rruleStr, "FREQ=YEARLY"))
    rule.freq = ICAL_FREQ_YEARLY;

  // Parse INTERVAL
  const char* intervalPtr = strstr(rruleStr, "INTERVAL=");
  if (intervalPtr) {
    rule.interval = atoi(intervalPtr + 9);
    if (rule.interval < 1) rule.interval = 1;
  }

  // Parse UNTIL
  const char* untilPtr = strstr(rruleStr, "UNTIL=");
  if (untilPtr) {
    rule.until = parseDate(untilPtr + 6);
  }

  // Parse COUNT
  const char* countPtr = strstr(rruleStr, "COUNT=");
  if (countPtr) {
    rule.count = atoi(countPtr + 6);
  }

  // Parse BYDAY for weekly recurrence
  const char* byDayPtr = strstr(rruleStr, "BYDAY=");
  if (byDayPtr) {
    if (strstr(byDayPtr, "SU")) rule.byDay |= (1 << 0);
    if (strstr(byDayPtr, "MO")) rule.byDay |= (1 << 1);
    if (strstr(byDayPtr, "TU")) rule.byDay |= (1 << 2);
    if (strstr(byDayPtr, "WE")) rule.byDay |= (1 << 3);
    if (strstr(byDayPtr, "TH")) rule.byDay |= (1 << 4);
    if (strstr(byDayPtr, "FR")) rule.byDay |= (1 << 5);
    if (strstr(byDayPtr, "SA")) rule.byDay |= (1 << 6);
  }

  return rule;
}

time_t ICalParser::getNextOccurrence(time_t dtstart, ICalRRule rule, time_t after) {
  if (rule.freq == ICAL_FREQ_NONE) {
    // Non-recurring: return dtstart if it's in the future
    return (dtstart >= after) ? dtstart : 0;
  }

  // Check if recurrence has ended by UNTIL date
  if (rule.until > 0 && after > rule.until) {
    return 0;
  }

  struct tm startTm;
  struct tm* startPtr = localtime(&dtstart);
  memcpy(&startTm, startPtr, sizeof(struct tm));

  time_t candidate = dtstart;
  int occurrenceCount = 1;  // First occurrence is the dtstart itself
  int iterations = 0;
  const int MAX_ITERATIONS = 2000;  // Safety limit

  // If first occurrence is already past 'after', return it
  if (candidate >= after) {
    return candidate;
  }

  while (iterations < MAX_ITERATIONS) {
    iterations++;

    switch (rule.freq) {
      case ICAL_FREQ_DAILY:
        startTm.tm_mday += rule.interval;
        break;
      case ICAL_FREQ_WEEKLY:
        if (rule.byDay == 0) {
          // Simple weekly, same day
          startTm.tm_mday += 7 * rule.interval;
        } else {
          // Weekly with specific days - advance one day at a time
          startTm.tm_mday += 1;
          time_t temp = mktime(&startTm);
          struct tm* tempTm = localtime(&temp);
          int wday = tempTm->tm_wday;  // 0=Sunday
          // Check if this day is in byDay mask
          if (!((rule.byDay >> wday) & 1)) {
            continue;  // Skip this day, don't count as occurrence
          }
        }
        break;
      case ICAL_FREQ_MONTHLY:
        startTm.tm_mon += rule.interval;
        break;
      case ICAL_FREQ_YEARLY:
        startTm.tm_year += rule.interval;
        break;
      default:
        return 0;
    }

    candidate = mktime(&startTm);
    occurrenceCount++;

    // Check COUNT limit - if we've exceeded the count, no more occurrences
    if (rule.count > 0 && occurrenceCount > rule.count) {
      return 0;
    }

    // Check UNTIL limit
    if (rule.until > 0 && candidate > rule.until) {
      return 0;
    }

    // Found a future occurrence
    if (candidate >= after) {
      return candidate;
    }
  }

  return 0;  // Exceeded max iterations
}

void ICalParser::insertSortedEvent(ICalEvent* events, int* count, int maxEvents,
                                   time_t occurrence, time_t endOccurrence, const char* title) {
  if (occurrence == 0) return;
  if (maxEvents > ICAL_MAX_EVENTS) maxEvents = ICAL_MAX_EVENTS;

  // Find insertion point
  int insertIdx = *count;
  for (int i = 0; i < *count; i++) {
    if (occurrence < events[i].occurrence) {
      insertIdx = i;
      break;
    }
  }

  // If list is full and this event is after all existing, skip it
  if (insertIdx >= maxEvents) return;

  // Shift events down to make room
  int shiftEnd = (*count < maxEvents) ? *count : maxEvents - 1;
  for (int i = shiftEnd; i > insertIdx; i--) {
    events[i] = events[i - 1];
  }

  // Insert new event
  events[insertIdx].occurrence = occurrence;
  events[insertIdx].endOccurrence = endOccurrence;

  struct tm* tmInfo = localtime(&occurrence);
  snprintf(events[insertIdx].datetime, 20, "%04d-%02d-%02d %02d:%02d",
           tmInfo->tm_year + 1900, tmInfo->tm_mon + 1, tmInfo->tm_mday,
           tmInfo->tm_hour, tmInfo->tm_min);

  struct tm* endTmInfo = localtime(&endOccurrence);
  snprintf(events[insertIdx].endDatetime, 20, "%04d-%02d-%02d %02d:%02d",
           endTmInfo->tm_year + 1900, endTmInfo->tm_mon + 1, endTmInfo->tm_mday,
           endTmInfo->tm_hour, endTmInfo->tm_min);

  strncpy(events[insertIdx].title, title, ICAL_MAX_TITLE_LEN - 1);
  events[insertIdx].title[ICAL_MAX_TITLE_LEN - 1] = '\0';

  if (*count < maxEvents) (*count)++;
}

ICalResult ICalParser::fetch(const char* url, time_t currentTime, int maxEvents) {
  ICalResult result = {{}, 0, 0, 0, false, 0, nullptr};

  if (maxEvents > ICAL_MAX_EVENTS) maxEvents = ICAL_MAX_EVENTS;

  if (!url || strlen(url) == 0) {
    result.errorMsg = "URL not set";
    return result;
  }

  if (WiFi.status() != WL_CONNECTED) {
    result.errorMsg = "No WiFi";
    return result;
  }

  debug("Fetching calendar...");
  debugf("Free heap: %d", ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(4096, 512);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.setTimeout(15000);

  if (!http.begin(client, url)) {
    result.errorMsg = "HTTP begin failed";
    return result;
  }

  http.addHeader("User-Agent", "ESP8266");
  http.addHeader("Accept", "*/*");

  debug("Sending request...");
  result.httpCode = http.GET();
  debugf("HTTP code: %d", result.httpCode);

  if (result.httpCode != 200) {
    http.end();
    result.errorMsg = "HTTP error";
    return result;
  }

  // Stream the response to avoid memory issues
  WiFiClient* stream = http.getStreamPtr();

  // Use provided current time
  time_t now = currentTime;

  String line = "";
  String currentDtStart = "";
  String currentDtEnd = "";
  String currentSummary = "";
  String currentRRule = "";
  bool inEvent = false;
  bool currentCancelled = false;
  bool hasRecurrenceId = false;

  debugf("Parsing calendar (next %d events)...", maxEvents);
  struct tm* nowTm = localtime(&now);
  debugf("Current: %04d-%02d-%02d %02d:%02d",
         nowTm->tm_year + 1900, nowTm->tm_mon + 1, nowTm->tm_mday,
         nowTm->tm_hour, nowTm->tm_min);

  unsigned long parseStart = millis();
  unsigned long lastData = millis();
  int lineCount = 0;

  while (millis() - parseStart < _parseTimeout) {
    if (stream->available()) {
      lastData = millis();
      char c = stream->read();

      if (c == '\n') {
        line.trim();
        lineCount++;

        if (line == "BEGIN:VEVENT") {
          inEvent = true;
          currentDtStart = "";
          currentDtEnd = "";
          currentSummary = "";
          currentRRule = "";
          currentCancelled = false;
          hasRecurrenceId = false;
        } else if (line == "END:VEVENT" && inEvent) {
          inEvent = false;
          result.totalEventsParsed++;

          // Skip cancelled events and recurrence modifications
          if (currentCancelled || hasRecurrenceId) {
            continue;
          }

          // Process the completed event
          if (currentDtStart.length() > 0 && currentSummary.length() > 0) {
            time_t dtstart = parseDate(currentDtStart.c_str());
            time_t dtend = (currentDtEnd.length() > 0) ? parseDate(currentDtEnd.c_str()) : dtstart;
            time_t duration = dtend - dtstart;  // Duration in seconds

            ICalRRule rule = parseRRule(currentRRule.c_str());

            if (rule.freq != ICAL_FREQ_NONE) {
              result.recurringEventsParsed++;
            }

            // Calculate next occurrence
            time_t nextOccur = getNextOccurrence(dtstart, rule, now);

            if (nextOccur > 0) {
              // Calculate end time by adding duration to the occurrence
              time_t endOccur = nextOccur + duration;
              insertSortedEvent(result.events, &result.eventCount, maxEvents,
                                nextOccur, endOccur, currentSummary.c_str());
            }
          }
        } else if (inEvent) {
          if (line.startsWith("DTSTART")) {
            int colonIdx = line.indexOf(':');
            if (colonIdx > 0) {
              currentDtStart = line.substring(colonIdx + 1);
              currentDtStart.trim();
            }
          } else if (line.startsWith("DTEND")) {
            int colonIdx = line.indexOf(':');
            if (colonIdx > 0) {
              currentDtEnd = line.substring(colonIdx + 1);
              currentDtEnd.trim();
            }
          } else if (line.startsWith("SUMMARY:")) {
            currentSummary = line.substring(8);
          } else if (line.startsWith("RRULE:")) {
            currentRRule = line.substring(6);
          } else if (line.startsWith("STATUS:")) {
            if (line.indexOf("CANCELLED") >= 0) {
              currentCancelled = true;
            }
          } else if (line.startsWith("RECURRENCE-ID")) {
            hasRecurrenceId = true;
          }
        }

        line = "";
        yield();  // Prevent watchdog timeout
      } else if (c != '\r') {
        if (line.length() < 256) {
          line += c;
        }
      }
    } else {
      // No data available
      if (millis() - lastData > _dataTimeout) {
        debug("Data timeout, stopping parse");
        break;
      }
      if (!http.connected()) {
        debug("Connection closed");
        break;
      }
      yield();
      delay(1);
    }
  }

  debugf("Parsed %d lines, %d events (%d recurring)",
         lineCount, result.totalEventsParsed, result.recurringEventsParsed);

  http.end();
  debugf("Found %d upcoming events", result.eventCount);

  result.success = true;
  return result;
}
