#include "AlarmView.h"
#include <stdio.h>

static View view;

// Display dimensions
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 160

// Layout: top 2/3 for bus, bottom 1/3 for controls
#define BUS_AREA_HEIGHT 100
#define CONTROL_AREA_Y 105

// Alarm state
static uint8_t alarm_hour = 7;
static uint8_t alarm_minute = 0;
static bool alarm_enabled = false;

// Animation state
static int anim_frame = 0;

// Draw the road with animated dashed lines
static void draw_road(int frame) {
  int road_y = 88;
  int road_height = 14;

  // Road surface
  gdispDrawLine(0, road_y, DISPLAY_WIDTH, road_y, White);
  gdispDrawLine(0, road_y + road_height, DISPLAY_WIDTH, road_y + road_height, White);

  // Animated center dashes - move right to left to simulate forward motion
  int dash_width = 15;
  int gap_width = 10;
  int total = dash_width + gap_width;
  int offset = (frame * 2) % total;  // Move 2 pixels per frame

  for (int x = -offset; x < DISPLAY_WIDTH; x += total) {
    int start_x = x;
    int end_x = x + dash_width;
    if (start_x < 0)
      start_x = 0;
    if (end_x > DISPLAY_WIDTH)
      end_x = DISPLAY_WIDTH;
    if (start_x < end_x) {
      gdispDrawLine(start_x, road_y + road_height / 2, end_x, road_y + road_height / 2, White);
    }
  }
}

// Draw the school bus (black and white, with bounce animation)
static void draw_school_bus(int frame) {
  // Slight vertical bounce
  int bounce = ((frame / 3) % 2);

  int bus_x = 15;
  int bus_y = 30 + bounce;
  int bus_width = 130;
  int bus_height = 50;

  // Main body - filled rectangle with outline
  gdispFillArea(bus_x, bus_y, bus_width, bus_height, White);
  gdispDrawBox(bus_x, bus_y, bus_width, bus_height, White);

  // Black stripe along bottom of bus
  gdispFillArea(bus_x, bus_y + bus_height - 6, bus_width, 6, Black);
  gdispDrawLine(bus_x, bus_y + bus_height - 6, bus_x + bus_width, bus_y + bus_height - 6, White);

  // Roof line (raised section)
  gdispDrawLine(bus_x + 3, bus_y - 3, bus_x + bus_width - 25, bus_y - 3, White);
  gdispDrawLine(bus_x + 3, bus_y - 3, bus_x + 3, bus_y, White);
  gdispDrawLine(bus_x + bus_width - 25, bus_y - 3, bus_x + bus_width - 25, bus_y, White);

  // Windows - 4 passenger windows (black rectangles on white body)
  int win_y = bus_y + 6;
  int win_height = 18;
  int win_width = 18;
  int win_gap = 4;

  for (int i = 0; i < 4; i++) {
    int win_x = bus_x + 6 + i * (win_width + win_gap);
    gdispFillArea(win_x, win_y, win_width, win_height, Black);
    gdispDrawBox(win_x, win_y, win_width, win_height, White);
  }

  // Front windshield (driver window)
  int front_win_x = bus_x + bus_width - 24;
  int front_win_y = bus_y + 6;
  gdispFillArea(front_win_x, front_win_y, 16, 16, Black);
  gdispDrawBox(front_win_x, front_win_y, 16, 16, White);

  // Door (between windows and front)
  int door_x = bus_x + 6 + 4 * (win_width + win_gap) - 2;
  int door_y = bus_y + 10;
  int door_height = bus_height - 16;
  gdispFillArea(door_x, door_y, 12, door_height, Black);
  gdispDrawBox(door_x, door_y, 12, door_height, White);
  // Door window
  gdispFillArea(door_x + 2, door_y + 2, 8, 10, Black);

  // Wheel arches (black semi-circles cut into body)
  int wheel_y = bus_y + bus_height;
  int rear_wheel_x = bus_x + 22;
  int front_wheel_x = bus_x + bus_width - 22;

  // Clear wheel arch areas
  gdispFillCircle(rear_wheel_x, wheel_y, 12, Black);
  gdispFillCircle(front_wheel_x, wheel_y, 12, Black);

  // Wheels - outer tire
  gdispDrawCircle(rear_wheel_x, wheel_y, 11, White);
  gdispDrawCircle(front_wheel_x, wheel_y, 11, White);

  // Wheels - inner hub
  gdispDrawCircle(rear_wheel_x, wheel_y, 6, White);
  gdispDrawCircle(front_wheel_x, wheel_y, 6, White);

  // Wheel spokes (rotate with animation)
  int sp = frame % 4;
  for (int w = 0; w < 2; w++) {
    int wx = (w == 0) ? rear_wheel_x : front_wheel_x;
    if (sp == 0 || sp == 2) {
      gdispDrawLine(wx, wheel_y - 5, wx, wheel_y + 5, White);
      gdispDrawLine(wx - 5, wheel_y, wx + 5, wheel_y, White);
    } else {
      gdispDrawLine(wx - 4, wheel_y - 4, wx + 4, wheel_y + 4, White);
      gdispDrawLine(wx - 4, wheel_y + 4, wx + 4, wheel_y - 4, White);
    }
  }

  // Front bumper
  gdispFillArea(bus_x + bus_width - 2, bus_y + bus_height - 8, 6, 8, White);

  // Rear bumper
  gdispFillArea(bus_x - 4, bus_y + bus_height - 8, 6, 8, White);

  // Headlights (front)
  gdispFillCircle(bus_x + bus_width + 1, bus_y + bus_height - 18, 3, White);
  gdispFillCircle(bus_x + bus_width + 1, bus_y + bus_height - 28, 3, White);

  // Tail lights (rear)
  gdispFillArea(bus_x - 3, bus_y + bus_height - 20, 3, 6, White);
  gdispFillArea(bus_x - 3, bus_y + bus_height - 30, 3, 6, White);

  // Stop sign arm (folded)
  gdispFillArea(bus_x - 6, bus_y + 8, 5, 12, White);
  gdispFillArea(bus_x - 5, bus_y + 9, 3, 10, Black);

  // "herndon" text on side of bus
  font_t font = gdispOpenFont("DejaVuSans10");
  gdispDrawString(bus_x + 38, bus_y + bus_height - 22, "Herndon", font, Black);
  gdispCloseFont(font);
}

// Draw on/off toggle switch (black and white)
static void draw_toggle_switch(int x, int y, bool is_on) {
  int switch_width = 50;
  int switch_height = 24;
  int knob_radius = 9;

  // Switch track outline
  gdispDrawBox(x, y, switch_width, switch_height, White);

  // Knob position - filled circle on the active side
  int knob_x = is_on ? (x + switch_width - knob_radius - 3) : (x + knob_radius + 3);
  int knob_y = y + switch_height / 2;
  gdispFillCircle(knob_x, knob_y, knob_radius, White);

  // ON/OFF label on opposite side of knob
  font_t font = gdispOpenFont("DejaVuSans12");
  if (is_on) {
    gdispDrawString(x + 5, y + 5, "ON", font, White);
  } else {
    gdispDrawString(x + switch_width - 24, y + 5, "OFF", font, White);
  }
  gdispCloseFont(font);
}

// Draw the time display
static void draw_alarm_time(void) {
  font_t font = gdispOpenFont("DejaVuSans24");

  // Format time as HH:MM AM/PM
  char time_str[12];
  uint8_t display_hour = alarm_hour;
  const char* ampm = "AM";

  if (display_hour == 0) {
    display_hour = 12;
    ampm = "AM";
  } else if (display_hour == 12) {
    ampm = "PM";
  } else if (display_hour > 12) {
    display_hour -= 12;
    ampm = "PM";
  }

  snprintf(time_str, sizeof(time_str), "%d:%02d%s", display_hour, alarm_minute, ampm);

  int text_x = 8;
  int text_y = CONTROL_AREA_Y + 15;

  gdispDrawString(text_x, text_y, time_str, font, White);
  gdispCloseFont(font);
}

static void render(void) {
  anim_frame++;

  gdispClear(Black);

  // Draw road first (background)
  draw_road(anim_frame);

  // Draw the school bus (top 2/3)
  draw_school_bus(anim_frame);

  // Draw separator line
  gdispDrawLine(5, CONTROL_AREA_Y - 2, DISPLAY_WIDTH - 5, CONTROL_AREA_Y - 2, White);

  // Draw alarm time (left side)
  draw_alarm_time();

  // Draw on/off toggle (right side, vertically centered in control area)
  draw_toggle_switch(DISPLAY_WIDTH - 58, CONTROL_AREA_Y + 15, alarm_enabled);

  gdispGFlush(gdispGetDisplay(0));
}

static void set_alarm_hour(uint8_t hour) {
  alarm_hour = hour % 24;
}

static void set_alarm_minute(uint8_t minute) {
  alarm_minute = minute % 60;
}

static void set_enabled(bool enabled) {
  alarm_enabled = enabled;
}

static bool is_enabled(void) {
  return alarm_enabled;
}

static uint8_t get_alarm_hour(void) {
  return alarm_hour;
}

static uint8_t get_alarm_minute(void) {
  return alarm_minute;
}

static View* init(void) {
  view.render = render;
  alarm_hour = 7;
  alarm_minute = 0;
  alarm_enabled = false;
  anim_frame = 0;
  return &view;
}

const struct alarmview AlarmView = {
    .init = init,
    .set_alarm_hour = set_alarm_hour,
    .set_alarm_minute = set_alarm_minute,
    .set_enabled = set_enabled,
    .is_enabled = is_enabled,
    .get_alarm_hour = get_alarm_hour,
    .get_alarm_minute = get_alarm_minute,
};
