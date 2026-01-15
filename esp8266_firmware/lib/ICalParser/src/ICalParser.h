#ifndef ICAL_PARSER_H
#define ICAL_PARSER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// Maximum events that can be returned
#define ICAL_MAX_EVENTS 20
#define ICAL_MAX_TITLE_LEN 64

// RRULE frequency types
typedef enum {
  ICAL_FREQ_NONE = 0,
  ICAL_FREQ_DAILY,
  ICAL_FREQ_WEEKLY,
  ICAL_FREQ_MONTHLY,
  ICAL_FREQ_YEARLY
} ICalFreq;

// Parsed RRULE
typedef struct {
  ICalFreq freq;
  int interval;      // Every N days/weeks/etc (default 1)
  time_t until;      // End date (0 = no end)
  int count;         // Max occurrences (0 = unlimited)
  uint8_t byDay;     // Bitmask: bit 0=SU, 1=MO, 2=TU, 3=WE, 4=TH, 5=FR, 6=SA
} ICalRRule;

// Calendar event
typedef struct {
  time_t occurrence;       // When this event occurs
  char datetime[20];       // Formatted: "YYYY-MM-DD HH:MM"
  char title[ICAL_MAX_TITLE_LEN];
} ICalEvent;

// Parser result
typedef struct {
  ICalEvent events[ICAL_MAX_EVENTS];
  int eventCount;
  int totalEventsParsed;
  int recurringEventsParsed;
  bool success;
  int httpCode;
  const char* errorMsg;
} ICalResult;

// Debug callback type
typedef void (*ICalDebugCallback)(const char* message);

class ICalParser {
public:
  ICalParser();

  // Set debug output callback
  void setDebugCallback(ICalDebugCallback callback);

  // Set parse timeout in milliseconds (default 30000)
  void setParseTimeout(unsigned long timeout);

  // Set data timeout in milliseconds (default 5000)
  void setDataTimeout(unsigned long timeout);

  // Fetch and parse calendar, returning up to maxEvents upcoming events
  // currentTime: epoch time for "now" (use NTPClient.getEpochTime())
  // Returns result structure with events and status
  ICalResult fetch(const char* url, time_t currentTime, int maxEvents = 10);

  // Parse iCal date string (YYYYMMDD or YYYYMMDDTHHMMSS or YYYYMMDDTHHMMSSZ)
  static time_t parseDate(const char* dateStr);

  // Parse RRULE string
  static ICalRRule parseRRule(const char* rruleStr);

  // Calculate next occurrence of recurring event after 'after' time
  static time_t getNextOccurrence(time_t dtstart, ICalRRule rule, time_t after);

private:
  ICalDebugCallback _debugCallback;
  unsigned long _parseTimeout;
  unsigned long _dataTimeout;

  void debug(const char* msg);
  void debugf(const char* fmt, ...);

  void insertSortedEvent(ICalEvent* events, int* count, int maxEvents,
                         time_t occurrence, const char* title);
};

#endif // ICAL_PARSER_H
