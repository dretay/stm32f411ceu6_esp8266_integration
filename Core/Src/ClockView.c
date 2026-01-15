#include "ClockView.h"
#include <math.h>   // for cos, sin
#include <stdio.h>  // for snprintf

static View view;

// Clock configuration constants
#define CLOCK_RADIUS 47
#define CLOCK_XOFFSET 83
#define CLOCK_YOFFSET 68
#define DEG_TO_RAD 0.017453293f  // pi/180
#define HOUR_HAND_LENGTH 0.7f
#define MIN_HAND_LENGTH 0.9f
#define SEC_HAND_LENGTH 0.9f

// Clock state
static struct {
  int point_x, point_y;
  int sec_arrow_x, sec_arrow_y;
  int min_arrow_x, min_arrow_y;
  int hour_arrow_x, hour_arrow_y;
  int prev_sec_x, prev_sec_y;
  int prev_min_x, prev_min_y;
  int prev_hour_x, prev_hour_y;
} clock_state = {0};

static float alfa;

static void draw_hour(char* hour_string, int x, int y) {
  font_t font = gdispOpenFont("DejaVuSansBold12");
  int hour_width = gdispGetStringWidth(hour_string, font) + 2;
  int hour_height = gdispGetFontMetric(font, fontHeight) + 1;
  int hour_x = x - (hour_width / 8) + 1;
  int hour_y = y - 1;

  if (strlen(hour_string) > 1) {
    hour_x -= 3;
  }

  gdispDrawStringBox(hour_x, hour_y, hour_width, hour_height, hour_string, font, White, justifyCenter);
  gdispCloseFont(font);
}
static void SetLines(int xl1pos, int yl1pos, int xl2pos, int yl2pos) {
  gdispDrawLine(xl1pos, yl1pos, xl2pos, yl2pos, White);
}

static void SetThickLines(int xl1pos, int yl1pos, int xl2pos, int yl2pos) {
  gdispDrawThickLine(xl1pos, yl1pos, xl2pos, yl2pos, White, 3, TRUE);
}
static void draw_clock(int hour, int minute, int second) {
  // Calculate hour hand position
  alfa = (270 + (30 * hour + (0.5 * minute))) * DEG_TO_RAD;
  clock_state.hour_arrow_x = (cos(alfa) * CLOCK_RADIUS * HOUR_HAND_LENGTH) + CLOCK_XOFFSET;
  clock_state.hour_arrow_y = (sin(alfa) * CLOCK_RADIUS * HOUR_HAND_LENGTH) + CLOCK_YOFFSET;

  // Calculate minute hand position
  alfa = (270 + (6 * minute)) * DEG_TO_RAD;
  clock_state.min_arrow_x = (cos(alfa) * CLOCK_RADIUS * MIN_HAND_LENGTH) + CLOCK_XOFFSET;
  clock_state.min_arrow_y = (sin(alfa) * CLOCK_RADIUS * MIN_HAND_LENGTH) + CLOCK_YOFFSET;

  // Calculate second hand position
  alfa = (270 + (6 * second)) * DEG_TO_RAD;
  clock_state.sec_arrow_x = (cos(alfa) * CLOCK_RADIUS * SEC_HAND_LENGTH) + CLOCK_XOFFSET;
  clock_state.sec_arrow_y = (sin(alfa) * CLOCK_RADIUS * SEC_HAND_LENGTH) + CLOCK_YOFFSET;

  // Draw hands
  SetThickLines(CLOCK_XOFFSET, CLOCK_YOFFSET, clock_state.hour_arrow_x, clock_state.hour_arrow_y);
  clock_state.prev_hour_x = clock_state.hour_arrow_x;
  clock_state.prev_hour_y = clock_state.hour_arrow_y;

  SetThickLines(CLOCK_XOFFSET, CLOCK_YOFFSET, clock_state.min_arrow_x, clock_state.min_arrow_y);
  clock_state.prev_min_x = clock_state.min_arrow_x;
  clock_state.prev_min_y = clock_state.min_arrow_y;

  SetLines(CLOCK_XOFFSET, CLOCK_YOFFSET, clock_state.sec_arrow_x, clock_state.sec_arrow_y);
  clock_state.prev_sec_x = clock_state.sec_arrow_x;
  clock_state.prev_sec_y = clock_state.sec_arrow_y;
}
static void SetPoint(int xppos, int yppos) {
  gdispDrawPixel(xppos, yppos, White);
}

static void SetFilledCircle(int xcpos, int ycpos, int radius) {
  gdispFillCircle(xcpos, ycpos, radius, White);
}

static void face() {
  for (int i = 0; i <= 59; i++) {
    alfa = 6 * i * DEG_TO_RAD;
    clock_state.point_x = (cos(alfa) * CLOCK_RADIUS) + CLOCK_XOFFSET;
    clock_state.point_y = (sin(alfa) * CLOCK_RADIUS) + CLOCK_YOFFSET;
    SetPoint(clock_state.point_x, clock_state.point_y);

    if ((i % 5) == 0) {
      SetFilledCircle(clock_state.point_x, clock_state.point_y, 2);
      alfa = 6 * (i + 45) * DEG_TO_RAD;
      clock_state.point_x = (cos(alfa) * (CLOCK_RADIUS + 12)) + CLOCK_XOFFSET - 5;
      clock_state.point_y = (sin(alfa) * (CLOCK_RADIUS + 12)) + CLOCK_YOFFSET - 5;

      char buffer[3] = {'\0'};
      int hour = (i / 5);
      if (hour == 0) {
        hour = 12;
      }
      snprintf(buffer, sizeof(buffer), "%d", hour);
      draw_hour(buffer, clock_state.point_x, clock_state.point_y);
    }
  }
  gdispDrawCircle(CLOCK_XOFFSET - 1, CLOCK_YOFFSET - 1, CLOCK_RADIUS + 20, White);
}

static void render() {
  coord_t width = gdispGetWidth();
  coord_t height = gdispGetHeight();
  RTC_TimeTypeDef currentTime;
  RTC_DateTypeDef currentDate;
  HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BIN);
  gdispClear(Black);

  // Draw clock face (static) and hands (dynamic)
  face();
  draw_clock(currentTime.Hours, currentTime.Minutes, currentTime.Seconds);

  // Draw time string at bottom
  font_t DejaVuSans24 = gdispOpenFont("DejaVuSans24");
  char time_string[10] = {'\0'};
  DateHelper.to_string(time_string);

  int time_width = gdispGetStringWidth(time_string, DejaVuSans24) + 1;
  int time_height = gdispGetFontMetric(DejaVuSans24, fontHeight) + 1;

  gdispDrawStringBox((width / 2) - (time_width / 2), height - time_height + 5, time_width, time_height, time_string,
                     DejaVuSans24, White, justifyCenter);
  gdispCloseFont(DejaVuSans24);

  gdispGFlush(gdispGetDisplay(0));
}

static View* init() {
  view.render = render;

  // Initialize hand positions
  clock_state.sec_arrow_x = CLOCK_XOFFSET;
  clock_state.sec_arrow_y = CLOCK_YOFFSET - CLOCK_RADIUS;
  clock_state.min_arrow_x = CLOCK_XOFFSET;
  clock_state.min_arrow_y = CLOCK_YOFFSET - CLOCK_RADIUS;
  clock_state.hour_arrow_x = CLOCK_XOFFSET;
  clock_state.hour_arrow_y = CLOCK_YOFFSET - CLOCK_RADIUS;

  return &view;
}

const struct clockview ClockView = {
    .init = init,
};
