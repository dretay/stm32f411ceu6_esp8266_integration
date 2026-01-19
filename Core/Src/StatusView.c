#include "StatusView.h"
#include <stdio.h>
#include <string.h>

static View view;

// Display dimensions
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 160

// Boot phase states
static boot_phase_state_t wifi_state = BOOT_PHASE_PENDING;
static boot_phase_state_t time_state = BOOT_PHASE_PENDING;
static boot_phase_state_t weather_state = BOOT_PHASE_PENDING;
static boot_phase_state_t balance_state = BOOT_PHASE_PENDING;
static boot_phase_state_t calendar_state = BOOT_PHASE_PENDING;

// Animation frame counter for spinner
static int anim_frame = 0;

// Draw a checkmark at position
static void draw_checkmark(int x, int y, int size) {
  // Simple checkmark: short line down-left, then longer line down-right
  int mid = size / 2;
  gdispDrawLine(x, y + mid, x + mid, y + size, White);
  gdispDrawLine(x + mid, y + size, x + size, y, White);
  // Make it thicker
  gdispDrawLine(x + 1, y + mid, x + mid + 1, y + size, White);
  gdispDrawLine(x + mid + 1, y + size, x + size + 1, y, White);
}

// Draw a spinning indicator - two rotating arrows (like a refresh icon)
static void draw_spinner(int x, int y, int size, int frame) {
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 1;

  // 8 rotation positions (every 45 degrees)
  // Each entry: {arc_start_x, arc_start_y, arc_end_x, arc_end_y, arrow_tip_x, arrow_tip_y}
  // Positions go clockwise from top
  static const int pos_x[] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int pos_y[] = {-1, -1, 0, 1, 1, 1, 0, -1};

  int rot = (frame / 3) % 8;  // Rotate every 3 frames

  // Draw two arrows 180 degrees apart
  for (int arrow = 0; arrow < 2; arrow++) {
    int base_pos = (rot + arrow * 4) % 8;  // Second arrow is opposite

    // Arrow arc endpoint
    int ax = cx + pos_x[base_pos] * r;
    int ay = cy + pos_y[base_pos] * r;

    // Arrow arc start (90 degrees behind)
    int start_pos = (base_pos + 6) % 8;  // 270 degrees ahead = 90 behind
    int sx = cx + pos_x[start_pos] * r;
    int sy = cy + pos_y[start_pos] * r;

    // Draw arc as lines through intermediate point
    int mid_pos = (base_pos + 7) % 8;  // 45 degrees behind
    int mx = cx + pos_x[mid_pos] * r;
    int my = cy + pos_y[mid_pos] * r;

    gdispDrawLine(sx, sy, mx, my, White);
    gdispDrawLine(mx, my, ax, ay, White);

    // Draw arrowhead - two small lines from tip
    int prev_pos = (base_pos + 7) % 8;

    // Arrowhead points inward and along the arc direction
    int head1_x = cx + pos_x[prev_pos] * (r - 3);
    int head1_y = cy + pos_y[prev_pos] * (r - 3);
    int head2_x = cx + pos_x[base_pos] * (r - 4);
    int head2_y = cy + pos_y[base_pos] * (r - 4);

    gdispDrawLine(ax, ay, head1_x, head1_y, White);
    gdispDrawLine(ax, ay, head2_x, head2_y, White);
  }
}

// Draw an empty circle (pending state)
static void draw_pending_circle(int x, int y, int size) {
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 1;
  gdispDrawCircle(cx, cy, r, HTML2COLOR(0x444444));
}

// Draw a status item with label and indicator
static void draw_status_item(int y, const char* label, boot_phase_state_t state) {
  font_t font = gdispOpenFont("DejaVuSans12");

  // Draw label
  int text_x = 40;
  gdispDrawString(text_x, y + 2, label, font, White);

  // Draw status indicator on the right
  int indicator_x = DISPLAY_WIDTH - 30;
  int indicator_size = 14;

  switch (state) {
    case BOOT_PHASE_COMPLETE:
      draw_checkmark(indicator_x, y + 2, indicator_size);
      break;
    case BOOT_PHASE_IN_PROGRESS:
      draw_spinner(indicator_x, y + 1, indicator_size, anim_frame);
      break;
    case BOOT_PHASE_PENDING:
    default:
      draw_pending_circle(indicator_x, y + 1, indicator_size);
      break;
  }

  gdispCloseFont(font);
}

static void render(void) {
  anim_frame++;

  gdispClear(Black);

  // Draw title
  font_t title_font = gdispOpenFont("DejaVuSans16");
  const char* title = "Booting...";
  int title_width = gdispGetStringWidth(title, title_font);
  gdispDrawString((DISPLAY_WIDTH - title_width) / 2, 20, title, title_font, White);
  gdispCloseFont(title_font);

  // Draw separator line
  gdispDrawLine(20, 45, DISPLAY_WIDTH - 20, 45, White);

  // Draw status items
  int item_y = 55;
  int item_spacing = 20;

  draw_status_item(item_y, "WiFi", wifi_state);
  item_y += item_spacing;

  draw_status_item(item_y, "Time", time_state);
  item_y += item_spacing;

  draw_status_item(item_y, "Weather", weather_state);
  item_y += item_spacing;

  draw_status_item(item_y, "Balance", balance_state);
  item_y += item_spacing;

  draw_status_item(item_y, "Calendar", calendar_state);

  gdispGFlush(gdispGetDisplay(0));
}

static void set_wifi_state(boot_phase_state_t state) {
  wifi_state = state;
}

static void set_time_state(boot_phase_state_t state) {
  time_state = state;
}

static void set_weather_state(boot_phase_state_t state) {
  weather_state = state;
}

static void set_balance_state(boot_phase_state_t state) {
  balance_state = state;
}

static void set_calendar_state(boot_phase_state_t state) {
  calendar_state = state;
}

static bool is_boot_complete(void) {
  return wifi_state == BOOT_PHASE_COMPLETE &&
         time_state == BOOT_PHASE_COMPLETE &&
         weather_state == BOOT_PHASE_COMPLETE &&
         balance_state == BOOT_PHASE_COMPLETE &&
         calendar_state == BOOT_PHASE_COMPLETE;
}

static View* init(void) {
  view.render = render;
  anim_frame = 0;
  wifi_state = BOOT_PHASE_PENDING;
  time_state = BOOT_PHASE_PENDING;
  weather_state = BOOT_PHASE_PENDING;
  balance_state = BOOT_PHASE_PENDING;
  calendar_state = BOOT_PHASE_PENDING;
  return &view;
}

const struct statusview StatusView = {
    .init = init,
    .set_wifi_state = set_wifi_state,
    .set_time_state = set_time_state,
    .set_weather_state = set_weather_state,
    .set_balance_state = set_balance_state,
    .set_calendar_state = set_calendar_state,
    .is_boot_complete = is_boot_complete,
};
