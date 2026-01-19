#include "FlipClockView.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "DateHelper.h"

static View view;

// Display dimensions
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 160

// Large digit dimensions (for time)
#define DIGIT_WIDTH 38
#define DIGIT_WIDTH_ONE 18  // Narrower width for "1" (only right segments)
#define DIGIT_HEIGHT 92
#define SEGMENT_THICK 10
#define DIGIT_SPACING 4

// Medium digit dimensions (for day/date)
#define MED_DIGIT_WIDTH 16
#define MED_DIGIT_HEIGHT 24
#define MED_SEGMENT_THICK 3
#define MED_DIGIT_SPACING 2

// Layout Y positions
#define TIME_Y 2
#define LINE1_Y 98
#define DATE_LABEL_Y 102
#define DATE_Y 112
#define LINE2_Y 138
#define TEMP_LABEL_Y 140
#define TEMP_Y 144

// Weather state
static flipclock_weather_t weather_data = {0};

// Animation state
static struct {
  int frame;
} anim = {0};

// Segment patterns for digits 0-9
// Bit order: 6=a(top), 5=b(upper-right), 4=c(lower-right), 3=d(bottom), 2=e(lower-left), 1=f(upper-left), 0=g(middle)
static const unsigned char digit_segments[10] = {
    0x7E,  // 0: a,b,c,d,e,f
    0x30,  // 1: b,c
    0x6D,  // 2: a,b,d,e,g
    0x79,  // 3: a,b,c,d,g
    0x33,  // 4: b,c,f,g
    0x5B,  // 5: a,c,d,f,g
    0x5F,  // 6: a,c,d,e,f,g
    0x70,  // 7: a,b,c
    0x7F,  // 8: all
    0x7B   // 9: a,b,c,d,f,g
};

// Draw a horizontal segment
static void draw_h_segment(int x, int y, int width, int thick) {
  for (int i = 0; i < thick; i++) {
    int inset = (i < thick / 2) ? (thick / 2 - i) : (i - thick / 2);
    gdispDrawLine(x + inset + 1, y + i, x + width - inset - 2, y + i, White);
  }
}

// Draw a vertical segment
static void draw_v_segment(int x, int y, int height, int thick) {
  for (int i = 0; i < thick; i++) {
    int x_off = x + i;
    int taper = (i < thick / 2) ? (thick / 2 - i) : (i - thick / 2);
    gdispDrawLine(x_off, y + taper + 1, x_off, y + height - taper - 2, White);
  }
}

// Draw a digit with configurable size
static void draw_digit_sized(int x, int y, int digit, int w, int h, int t) {
  if (digit < 0 || digit > 9)
    return;
  unsigned char segs = digit_segments[digit];
  int half_h = h / 2;

  if (segs & 0x40)
    draw_h_segment(x + t / 2, y, w - t, t);  // a - top
  if (segs & 0x20)
    draw_v_segment(x + w - t, y + t / 2, half_h - t / 2, t);  // b - upper right
  if (segs & 0x10)
    draw_v_segment(x + w - t, y + half_h, half_h - t / 2, t);  // c - lower right
  if (segs & 0x08)
    draw_h_segment(x + t / 2, y + h - t, w - t, t);  // d - bottom
  if (segs & 0x04)
    draw_v_segment(x, y + half_h, half_h - t / 2, t);  // e - lower left
  if (segs & 0x02)
    draw_v_segment(x, y + t / 2, half_h - t / 2, t);  // f - upper left
  if (segs & 0x01)
    draw_h_segment(x + t / 2, y + half_h - t / 2, w - t, t);  // g - middle
}

// Draw large digit for time
static void draw_large_digit(int x, int y, int digit) {
  draw_digit_sized(x, y, digit, DIGIT_WIDTH, DIGIT_HEIGHT, SEGMENT_THICK);
}

// Draw medium digit for day/date
static void draw_med_digit(int x, int y, int digit) {
  draw_digit_sized(x, y, digit, MED_DIGIT_WIDTH, MED_DIGIT_HEIGHT, MED_SEGMENT_THICK);
}

// Draw colon between hours and minutes (blinks based on seconds)
static void draw_colon(int x, int y, int height, int seconds) {
  // Blink on even seconds
  if (seconds % 2 == 0) {
    int dot_r = 3;
    int spacing = height / 4;
    // Center dots within the colon width (8 pixels)
    gdispFillCircle(x + 4, y + height / 2 - spacing, dot_r, White);
    gdispFillCircle(x + 4, y + height / 2 + spacing, dot_r, White);
  }
}

// Draw horizontal separator line
static void draw_separator(int y) {
  gdispDrawLine(8, y, DISPLAY_WIDTH - 8, y, White);
}

// Draw time (large HH:MM) - centered
static void draw_time(int hours, int minutes, int seconds) {
  int display_hours = hours % 12;
  if (display_hours == 0)
    display_hours = 12;

  int h_tens = display_hours / 10;
  int h_ones = display_hours % 10;
  int m_tens = minutes / 10;
  int m_ones = minutes % 10;

  int colon_width = 8;

  // Calculate total width and starting position
  // In 12-hour format, h_tens is always 1 (for 10, 11, 12), so use narrower width
  int total_width;
  int x;
  if (h_tens > 0) {
    // Leading "1" uses narrower width since it only has right-side segments
    total_width = DIGIT_WIDTH_ONE + DIGIT_SPACING + 3 * DIGIT_WIDTH + DIGIT_SPACING + colon_width;
    x = (DISPLAY_WIDTH - total_width) / 2 - 5;
  } else {
    // 3-digit time (1-9): center properly
    total_width = 3 * DIGIT_WIDTH + DIGIT_SPACING + colon_width;
    x = (DISPLAY_WIDTH - total_width) / 2;
  }

  // Draw hour digits
  if (h_tens > 0) {
    // Draw leading "1" with narrower width so segments align properly
    draw_digit_sized(x, TIME_Y, h_tens, DIGIT_WIDTH_ONE, DIGIT_HEIGHT, SEGMENT_THICK);
    x += DIGIT_WIDTH_ONE + DIGIT_SPACING;
  }
  draw_large_digit(x, TIME_Y, h_ones);
  x += DIGIT_WIDTH;

  // Draw colon (blinks each second)
  draw_colon(x, TIME_Y, DIGIT_HEIGHT, seconds);
  x += colon_width;

  // Draw minute digits
  draw_large_digit(x, TIME_Y, m_tens);
  x += DIGIT_WIDTH + DIGIT_SPACING;
  draw_large_digit(x, TIME_Y, m_ones);
}

// Draw slash between month and date
static void draw_slash(int x, int y, int height) {
  gdispDrawLine(x + 6, y, x, y + height, White);
  gdispDrawLine(x + 7, y, x + 1, y + height, White);
}

// Draw date section with labels above each value
static void draw_date(void) {
  RTC_DateTypeDef currentDate;
  RTC_TimeTypeDef dummyTime;
  HAL_RTC_GetTime(&hrtc, &dummyTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BIN);

  // Get 3-letter day abbreviation
  static const char* day_abbrev[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  const char* day = day_abbrev[currentDate.WeekDay % 7];

  int month = currentDate.Month;
  int date = currentDate.Date;

  font_t label_font = gdispOpenFont("DejaVuSans10");
  font_t value_font = gdispOpenFont("DejaVuSans16");

  // DAY section (left side) - center label over value
  int day_value_x = 3;
  int day_value_width = gdispGetStringWidth(day, value_font);
  int day_label_width = gdispGetStringWidth("DAY", label_font);
  int day_label_x = day_value_x + (day_value_width - day_label_width) / 2;
  gdispDrawString(day_label_x, DATE_LABEL_Y, "DAY", label_font, White);
  gdispDrawString(day_value_x, DATE_Y + 4, day, value_font, White);

  // Right side: MONTH and DATE with values below
  int right_offset = 20;

  // Position month label and digits
  int month_label_x = 60 + right_offset;
  gdispDrawString(month_label_x, DATE_LABEL_Y, "MONTH", label_font, White);

  int month_x = 65 + right_offset;
  if (month >= 10) {
    draw_med_digit(month_x, DATE_Y, month / 10);
    month_x += MED_DIGIT_WIDTH + MED_DIGIT_SPACING;
  }
  draw_med_digit(month_x, DATE_Y, month % 10);
  month_x += MED_DIGIT_WIDTH + 2;

  // Slash
  draw_slash(month_x, DATE_Y, MED_DIGIT_HEIGHT);
  int date_x = month_x + 10;

  // DATE label and digits
  gdispDrawString(date_x + 5, DATE_LABEL_Y, "DATE", label_font, White);
  if (date >= 10) {
    draw_med_digit(date_x, DATE_Y, date / 10);
    date_x += MED_DIGIT_WIDTH + MED_DIGIT_SPACING;
  }
  draw_med_digit(date_x, DATE_Y, date % 10);

  gdispCloseFont(label_font);
  gdispCloseFont(value_font);
}

// Draw rain drop icon
static void draw_rain_icon(int x, int y, int size) {
  int half = size / 2;
  // Teardrop shape - pointed top, round bottom
  for (int i = 0; i < half; i++) {
    int width = (i * half) / (half > 0 ? half : 1);
    if (width > 0) {
      gdispDrawLine(x + half - width, y + i, x + half + width, y + i, White);
    }
  }
  gdispFillCircle(x + half, y + half + 2, half, White);
}

// Draw snowflake icon
static void draw_snow_icon(int x, int y, int size) {
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 1;
  // Draw 6-pointed star (3 lines through center)
  gdispDrawLine(cx, cy - r, cx, cy + r, White);                  // vertical
  gdispDrawLine(cx - r, cy - r / 2, cx + r, cy + r / 2, White);  // diagonal 1
  gdispDrawLine(cx - r, cy + r / 2, cx + r, cy - r / 2, White);  // diagonal 2
  // Small crossbars on each arm
  int cb = r / 3;
  gdispDrawLine(cx - cb, cy - r + cb, cx + cb, cy - r + cb, White);
  gdispDrawLine(cx - cb, cy + r - cb, cx + cb, cy + r - cb, White);
}

// Draw sleet icon (rain + snow mix)
static void draw_sleet_icon(int x, int y, int size) {
  // Small raindrop on left
  int half = size / 4;
  int rx = x + 1;
  int ry = y + size / 3;
  for (int i = 0; i < half; i++) {
    int width = (i * half) / (half > 0 ? half : 1);
    if (width > 0) {
      gdispDrawLine(rx + half - width, ry + i, rx + half + width, ry + i, White);
    }
  }
  gdispFillCircle(rx + half, ry + half + 1, half, White);
  // Small snowflake on right
  int sx = x + size / 2 + 1;
  int sy = y + 2;
  int sr = size / 4;
  gdispDrawLine(sx, sy, sx, sy + sr * 2, White);
  gdispDrawLine(sx - sr, sy + sr / 2, sx + sr, sy + sr + sr / 2, White);
  gdispDrawLine(sx - sr, sy + sr + sr / 2, sx + sr, sy + sr / 2, White);
}

// Draw degree symbol manually (small circle)
static void draw_degree_symbol(int x, int y) {
  gdispDrawCircle(x + 2, y + 3, 2, White);
}

// Draw sun icon
static void draw_sun_icon(int x, int y, int size) {
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 3;
  // Draw circle for sun body
  gdispFillCircle(cx, cy, r, White);
  // Draw rays
  int ray_len = size / 4;
  int ray_start = r + 1;
  gdispDrawLine(cx, cy - ray_start, cx, cy - ray_start - ray_len, White);  // top
  gdispDrawLine(cx, cy + ray_start, cx, cy + ray_start + ray_len, White);  // bottom
  gdispDrawLine(cx - ray_start, cy, cx - ray_start - ray_len, cy, White);  // left
  gdispDrawLine(cx + ray_start, cy, cx + ray_start + ray_len, cy, White);  // right
}

// Draw moon icon (crescent)
static void draw_moon_icon(int x, int y, int size) {
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 1;
  // Draw filled circle
  gdispFillCircle(cx, cy, r, White);
  // Cut out a smaller circle to create crescent (draw in black)
  gdispFillCircle(cx + r / 2, cy - r / 4, r - 1, Black);
}

// Determine precipitation type from condition string
typedef enum { PRECIP_NONE, PRECIP_RAIN, PRECIP_SNOW, PRECIP_SLEET } precip_type_t;

static precip_type_t get_precip_type(const char* condition) {
  if (strstr(condition, "Sleet") != NULL || strstr(condition, "sleet") != NULL || strstr(condition, "Ice") != NULL ||
      strstr(condition, "ice") != NULL || strstr(condition, "Freezing") != NULL ||
      strstr(condition, "freezing") != NULL || strstr(condition, "Wintry") != NULL ||
      strstr(condition, "wintry") != NULL) {
    return PRECIP_SLEET;
  }
  if (strstr(condition, "Snow") != NULL || strstr(condition, "snow") != NULL || strstr(condition, "Flurr") != NULL ||
      strstr(condition, "flurr") != NULL || strstr(condition, "Blizzard") != NULL ||
      strstr(condition, "blizzard") != NULL) {
    return PRECIP_SNOW;
  }
  if (strstr(condition, "Rain") != NULL || strstr(condition, "rain") != NULL || strstr(condition, "Drizzle") != NULL ||
      strstr(condition, "drizzle") != NULL || strstr(condition, "Shower") != NULL ||
      strstr(condition, "shower") != NULL || strstr(condition, "Thunder") != NULL ||
      strstr(condition, "thunder") != NULL) {
    return PRECIP_RAIN;
  }
  return PRECIP_NONE;
}

// Draw temperature section with temp, condition, icon, precipitation
static void draw_temp(void) {
  font_t font = gdispOpenFont("DejaVuSans16");
  int text_x = 4;

  if (!weather_data.valid) {
    // Show placeholder when no data
    gdispDrawString(text_x, TEMP_Y + 1, "--", font, White);
    int dash_width = gdispGetStringWidth("--", font);
    draw_degree_symbol(text_x + dash_width, TEMP_Y + 1);
    gdispDrawString(text_x + dash_width + 8, TEMP_Y + 1, "F", font, White);
    gdispCloseFont(font);
    return;
  }

  // Draw temperature value
  char temp_val[8];
  snprintf(temp_val, sizeof(temp_val), "%d", weather_data.temp_f);
  gdispDrawString(text_x, TEMP_Y + 1, temp_val, font, White);
  int temp_width = gdispGetStringWidth(temp_val, font);

  // Draw degree symbol manually
  draw_degree_symbol(text_x + temp_width, TEMP_Y + 1);

  // Draw F
  gdispDrawString(text_x + temp_width + 8, TEMP_Y + 1, "F", font, White);
  int f_width = gdispGetStringWidth("F", font);
  int total_temp_width = temp_width + 8 + f_width;

  // Draw condition/forecast (truncate to fit before icon)
  char condition_short[10];
  strncpy(condition_short, weather_data.condition, sizeof(condition_short) - 1);
  condition_short[sizeof(condition_short) - 1] = '\0';

  gdispDrawString(text_x + total_temp_width + 8, TEMP_Y + 1, condition_short, font, White);

  // Icon and percentage at far right
  int icon_x = DISPLAY_WIDTH - 38;
  int precip_x = DISPLAY_WIDTH - 24;

  // Determine precipitation type for icon
  precip_type_t precip = get_precip_type(weather_data.condition);

  // Get current hour to determine day/night
  RTC_TimeTypeDef currentTime;
  RTC_DateTypeDef currentDate;
  HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BIN);
  bool is_day = (currentTime.Hours >= 6 && currentTime.Hours < 18);

  // Always draw weather icon
  switch (precip) {
    case PRECIP_SNOW:
      draw_snow_icon(icon_x, TEMP_Y, 10);
      break;
    case PRECIP_SLEET:
      draw_sleet_icon(icon_x, TEMP_Y, 10);
      break;
    case PRECIP_RAIN:
      draw_rain_icon(icon_x, TEMP_Y, 10);
      break;
    case PRECIP_NONE:
    default:
      // Show sun during day, moon at night
      if (is_day) {
        draw_sun_icon(icon_x, TEMP_Y, 10);
      } else {
        draw_moon_icon(icon_x, TEMP_Y, 10);
      }
      break;
  }

  // Always draw precipitation percentage for consistent layout
  char precip_str[8];
  snprintf(precip_str, sizeof(precip_str), "%d%%", weather_data.precip_chance);
  font_t precip_font = gdispOpenFont("DejaVuSans10");
  gdispDrawString(precip_x, TEMP_Y + 3, precip_str, precip_font, White);
  gdispCloseFont(precip_font);

  gdispCloseFont(font);
}

static void render(void) {
  RTC_TimeTypeDef currentTime;
  RTC_DateTypeDef currentDate;

  HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BIN);

  anim.frame++;

  gdispClear(Black);

  // Draw time (pass seconds for colon blink)
  draw_time(currentTime.Hours, currentTime.Minutes, currentTime.Seconds);

  // Draw separator line 1
  draw_separator(LINE1_Y);

  // Draw date section
  draw_date();

  // Draw separator line 2
  draw_separator(LINE2_Y);

  // Draw temperature section
  draw_temp();

  gdispGFlush(gdispGetDisplay(0));
}

static void set_weather(int16_t temp_f, const char* condition, uint8_t precip_chance) {
  weather_data.temp_f = temp_f;
  if (condition) {
    strncpy(weather_data.condition, condition, sizeof(weather_data.condition) - 1);
    weather_data.condition[sizeof(weather_data.condition) - 1] = '\0';
  } else {
    weather_data.condition[0] = '\0';
  }
  weather_data.precip_chance = precip_chance;
  weather_data.valid = true;
}

static View* init(void) {
  view.render = render;
  anim.frame = 0;
  weather_data.valid = false;
  return &view;
}

const struct flipclockview FlipClockView = {
    .init = init,
    .set_weather = set_weather,
};
