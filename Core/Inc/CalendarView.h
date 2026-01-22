#pragma once

#include "View.h"
#include <stdbool.h>
#include <stdint.h>

#define CALENDAR_MAX_EVENTS 10
#define CALENDAR_MAX_TITLE_LEN 64

typedef struct {
  char start[20];  // YYYY-MM-DD HH:MM
  char end[20];    // YYYY-MM-DD HH:MM
  char title[CALENDAR_MAX_TITLE_LEN];
} calendar_event_t;

struct calendarview {
  View* (*init)(void);
  void (*set_events)(calendar_event_t* events, uint8_t count);
};
extern const struct calendarview CalendarView;
