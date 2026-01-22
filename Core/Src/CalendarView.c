#include "CalendarView.h"
#include "DateHelper.h"
#include <stdio.h>
#include <string.h>

static View view;

// Display dimensions
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 160

// Layout
#define HEADER_HEIGHT 22
#define EVENT_HEIGHT 34
#define EVENT_MARGIN 3
#define MAX_VISIBLE_EVENTS 4

// Stored calendar data
static calendar_event_t events[CALENDAR_MAX_EVENTS];
static uint8_t event_count = 0;

// Format time from "YYYY-MM-DD HH:MM" to "10:30a" (converts UTC to Eastern)
static void format_time_only(const char* datetime, char* output, size_t output_size) {
  int year, month, day, hour, minute;
  if (sscanf(datetime, "%d-%d-%d %d:%d", &year, &month, &day, &hour, &minute) != 5) {
    strncpy(output, "?", output_size - 1);
    output[output_size - 1] = '\0';
    return;
  }

  // Convert UTC to Eastern time
  uint16_t local_year = year;
  uint8_t local_month = month;
  uint8_t local_day = day;
  uint8_t local_hour = hour;
  DateHelper.apply_tz_offset_eastern(&local_year, &local_month, &local_day, &local_hour);

  // Convert to 12-hour format
  const char* ampm = "a";
  int display_hour = local_hour;
  if (local_hour == 0) {
    display_hour = 12;
  } else if (local_hour == 12) {
    ampm = "p";
  } else if (local_hour > 12) {
    display_hour = local_hour - 12;
    ampm = "p";
  }

  snprintf(output, output_size, "%d:%02d%s", display_hour, minute, ampm);
}

// Get day abbreviation from datetime
static const char* get_day_abbrev(const char* datetime) {
  static const char* day_abbrev[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  int year, month, day, hour, minute;
  if (sscanf(datetime, "%d-%d-%d %d:%d", &year, &month, &day, &hour, &minute) != 5) {
    return "???";
  }

  // Convert to local time first
  uint16_t local_year = year;
  uint8_t local_month = month;
  uint8_t local_day = day;
  uint8_t local_hour = hour;
  DateHelper.apply_tz_offset_eastern(&local_year, &local_month, &local_day, &local_hour);

  uint8_t dow = DateHelper.calc_day_of_week(local_year, local_month, local_day);
  return day_abbrev[dow];
}

// Draw a bullet point
static void draw_bullet(int x, int y) {
  gdispFillCircle(x, y, 2, White);
}

// Draw a clock icon
static void draw_clock_icon(int x, int y, int size) {
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 1;

  // Clock face
  gdispDrawCircle(cx, cy, r, White);

  // Clock hands (pointing to ~10:10 for visual appeal)
  gdispDrawLine(cx, cy, cx, cy - r + 2, White);      // minute hand (12 o'clock)
  gdispDrawLine(cx, cy, cx - r + 4, cy - 2, White);  // hour hand (~10 o'clock)

  // Center dot
  gdispFillCircle(cx, cy, 1, White);
}

// Draw a single calendar event
static void draw_event(int y, const calendar_event_t* event, int index) {
  int content_x = 8;

  // Draw bullet point
  draw_bullet(content_x, y + 8);

  // Time range with clock icon
  font_t time_font = gdispOpenFont("DejaVuSans10");
  char start_str[12], end_str[12];
  format_time_only(event->start, start_str, sizeof(start_str));
  format_time_only(event->end, end_str, sizeof(end_str));

  char time_range[32];
  snprintf(time_range, sizeof(time_range), "%s  %s - %s", get_day_abbrev(event->start), start_str, end_str);
  gdispDrawString(content_x + 8, y + 2, time_range, time_font, White);
  gdispCloseFont(time_font);

  // Title on second line (larger)
  font_t title_font = gdispOpenFont("DejaVuSans12");

  // Truncate title if needed
  char title_display[24];
  strncpy(title_display, event->title, sizeof(title_display) - 1);
  title_display[sizeof(title_display) - 1] = '\0';

  int max_width = DISPLAY_WIDTH - content_x - 12;
  int title_width = gdispGetStringWidth(title_display, title_font);
  if (title_width > max_width) {
    size_t len = strlen(title_display);
    while (len > 3 && gdispGetStringWidth(title_display, title_font) > max_width) {
      len--;
      title_display[len] = '\0';
    }
    if (len > 3) {
      strcpy(&title_display[len - 2], "..");
    }
  }

  gdispDrawString(content_x + 8, y + 16, title_display, title_font, White);
  gdispCloseFont(title_font);

  // Dotted separator line below event (except for last visible)
  int line_y = y + EVENT_HEIGHT - 2;
  for (int dx = 8; dx < DISPLAY_WIDTH - 8; dx += 4) {
    gdispDrawPixel(dx, line_y, White);
  }
}

// Draw decorative header with calendar icon
static void draw_header(void) {
  // Calendar icon (left side)
  int icon_x = 6;
  int icon_y = 3;
  int icon_w = 14;
  int icon_h = 12;

  // Calendar body
  gdispDrawBox(icon_x, icon_y + 2, icon_w, icon_h, White);

  // Calendar top bar
  gdispDrawLine(icon_x, icon_y + 5, icon_x + icon_w - 1, icon_y + 5, White);

  // Calendar rings
  gdispDrawLine(icon_x + 3, icon_y, icon_x + 3, icon_y + 4, White);
  gdispDrawLine(icon_x + 10, icon_y, icon_x + 10, icon_y + 4, White);

  // Small dots for calendar grid
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 3; col++) {
      gdispDrawPixel(icon_x + 3 + col * 4, icon_y + 8 + row * 3, White);
    }
  }

  // Title
  font_t title_font = gdispOpenFont("DejaVuSans16");
  gdispDrawString(26, 1, "Schedule", title_font, White);
  gdispCloseFont(title_font);

  // Double line separator
  gdispDrawLine(6, HEADER_HEIGHT - 3, DISPLAY_WIDTH - 6, HEADER_HEIGHT - 3, White);
  gdispDrawLine(6, HEADER_HEIGHT - 1, DISPLAY_WIDTH - 6, HEADER_HEIGHT - 1, White);
}

static void render(void) {
  gdispClear(Black);

  draw_header();

  if (event_count == 0) {
    // Empty state
    font_t font = gdispOpenFont("DejaVuSans12");

    // Draw empty calendar icon (larger)
    int cx = DISPLAY_WIDTH / 2;
    int cy = 70;

    // Calendar outline
    gdispDrawBox(cx - 20, cy - 14, 40, 32, White);
    gdispDrawLine(cx - 20, cy - 6, cx + 19, cy - 6, White);

    // Calendar rings
    gdispDrawLine(cx - 12, cy - 18, cx - 12, cy - 12, White);
    gdispDrawLine(cx + 12, cy - 18, cx + 12, cy - 12, White);

    // X mark inside (no events)
    gdispDrawLine(cx - 10, cy, cx + 10, cy + 14, White);
    gdispDrawLine(cx + 10, cy, cx - 10, cy + 14, White);

    const char* msg = "No upcoming events";
    int msg_width = gdispGetStringWidth(msg, font);
    gdispDrawString((DISPLAY_WIDTH - msg_width) / 2, cy + 28, msg, font, White);
    gdispCloseFont(font);
  } else {
    // Draw events
    int y = HEADER_HEIGHT + EVENT_MARGIN;
    int visible = (event_count < MAX_VISIBLE_EVENTS) ? event_count : MAX_VISIBLE_EVENTS;

    for (int i = 0; i < visible; i++) {
      draw_event(y, &events[i], i);
      y += EVENT_HEIGHT;
    }

    // More events indicator with arrow
    if (event_count > MAX_VISIBLE_EVENTS) {
      font_t font = gdispOpenFont("DejaVuSans10");
      char more_str[16];
      snprintf(more_str, sizeof(more_str), "+%d more", event_count - MAX_VISIBLE_EVENTS);
      int text_width = gdispGetStringWidth(more_str, font);
      int text_x = DISPLAY_WIDTH - text_width - 10;
      int text_y = DISPLAY_HEIGHT - 14;

      // Down arrow
      int arrow_x = text_x - 10;
      int arrow_y = text_y + 4;
      gdispDrawLine(arrow_x, arrow_y, arrow_x + 4, arrow_y + 4, White);
      gdispDrawLine(arrow_x + 8, arrow_y, arrow_x + 4, arrow_y + 4, White);

      gdispDrawString(text_x, text_y, more_str, font, White);
      gdispCloseFont(font);
    }
  }

  gdispGFlush(gdispGetDisplay(0));
}

static void set_events(calendar_event_t* new_events, uint8_t count) {
  event_count = (count > CALENDAR_MAX_EVENTS) ? CALENDAR_MAX_EVENTS : count;
  for (int i = 0; i < event_count; i++) {
    strncpy(events[i].start, new_events[i].start, sizeof(events[i].start) - 1);
    events[i].start[sizeof(events[i].start) - 1] = '\0';
    strncpy(events[i].end, new_events[i].end, sizeof(events[i].end) - 1);
    events[i].end[sizeof(events[i].end) - 1] = '\0';
    strncpy(events[i].title, new_events[i].title, sizeof(events[i].title) - 1);
    events[i].title[sizeof(events[i].title) - 1] = '\0';
  }
}

static View* init(void) {
  view.render = render;
  event_count = 0;
  memset(events, 0, sizeof(events));
  return &view;
}

const struct calendarview CalendarView = {
    .init = init,
    .set_events = set_events,
};
