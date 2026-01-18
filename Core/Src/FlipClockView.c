#include "FlipClockView.h"
#include "DateHelper.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static View view;

// Display dimensions
#define DISPLAY_WIDTH  160
#define DISPLAY_HEIGHT 160

// Large digit dimensions (for time)
#define DIGIT_WIDTH    32
#define DIGIT_HEIGHT   52
#define SEGMENT_THICK  6
#define DIGIT_SPACING  4

// Medium digit dimensions (for day/date)
#define MED_DIGIT_WIDTH   18
#define MED_DIGIT_HEIGHT  28
#define MED_SEGMENT_THICK 4
#define MED_DIGIT_SPACING 2

// Layout Y positions
#define TIME_Y         6
#define LINE1_Y        62
#define DATE_LABEL_Y   66
#define DATE_Y         78
#define LINE2_Y        110
#define TEMP_LABEL_Y   114
#define TEMP_Y         126

// Weather state
static flipclock_weather_t weather_data = {0};

// Animation state
static struct {
  int frame;
} anim = {0};

// Segment patterns for digits 0-9
// Bit order: 6=a(top), 5=b(upper-right), 4=c(lower-right), 3=d(bottom), 2=e(lower-left), 1=f(upper-left), 0=g(middle)
static const unsigned char digit_segments[10] = {
  0x7E, // 0: a,b,c,d,e,f
  0x30, // 1: b,c
  0x6D, // 2: a,b,d,e,g
  0x79, // 3: a,b,c,d,g
  0x33, // 4: b,c,f,g
  0x5B, // 5: a,c,d,f,g
  0x5F, // 6: a,c,d,e,f,g
  0x70, // 7: a,b,c
  0x7F, // 8: all
  0x7B  // 9: a,b,c,d,f,g
};

// Draw a horizontal segment
static void draw_h_segment(int x, int y, int width, int thick) {
  for (int i = 0; i < thick; i++) {
    int inset = (i < thick/2) ? (thick/2 - i) : (i - thick/2);
    gdispDrawLine(x + inset + 1, y + i, x + width - inset - 2, y + i, White);
  }
}

// Draw a vertical segment
static void draw_v_segment(int x, int y, int height, int thick) {
  for (int i = 0; i < thick; i++) {
    int x_off = x + i;
    int taper = (i < thick/2) ? (thick/2 - i) : (i - thick/2);
    gdispDrawLine(x_off, y + taper + 1, x_off, y + height - taper - 2, White);
  }
}

// Draw a digit with configurable size
static void draw_digit_sized(int x, int y, int digit, int w, int h, int t) {
  if (digit < 0 || digit > 9) return;
  unsigned char segs = digit_segments[digit];
  int half_h = h / 2;

  if (segs & 0x40) draw_h_segment(x + t/2, y, w - t, t);                    // a - top
  if (segs & 0x20) draw_v_segment(x + w - t, y + t/2, half_h - t/2, t);     // b - upper right
  if (segs & 0x10) draw_v_segment(x + w - t, y + half_h, half_h - t/2, t);  // c - lower right
  if (segs & 0x08) draw_h_segment(x + t/2, y + h - t, w - t, t);            // d - bottom
  if (segs & 0x04) draw_v_segment(x, y + half_h, half_h - t/2, t);          // e - lower left
  if (segs & 0x02) draw_v_segment(x, y + t/2, half_h - t/2, t);             // f - upper left
  if (segs & 0x01) draw_h_segment(x + t/2, y + half_h - t/2, w - t, t);     // g - middle
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
    gdispFillCircle(x + 4, y + height/2 - spacing, dot_r, White);
    gdispFillCircle(x + 4, y + height/2 + spacing, dot_r, White);
  }
}

// Draw horizontal separator line
static void draw_separator(int y) {
  gdispDrawLine(8, y, DISPLAY_WIDTH - 8, y, White);
}

// Draw thermometer icon
static void draw_thermometer(int x, int y, int height) {
  int bulb_r = height / 5;
  int stem_w = bulb_r;
  int stem_h = height - bulb_r * 2;
  int stem_x = x + bulb_r - stem_w/2;

  // Draw stem (outline)
  gdispDrawLine(stem_x, y, stem_x, y + stem_h, White);
  gdispDrawLine(stem_x + stem_w, y, stem_x + stem_w, y + stem_h, White);
  gdispDrawLine(stem_x, y, stem_x + stem_w, y, White);

  // Draw bulb (circle outline)
  gdispDrawCircle(x + bulb_r, y + stem_h + bulb_r, bulb_r, White);

  // Fill indicator (partial fill to show temperature)
  gdispFillCircle(x + bulb_r, y + stem_h + bulb_r, bulb_r - 2, White);
  gdispFillArea(stem_x + 2, y + stem_h/2, stem_w - 3, stem_h/2 + 2, White);
}

// Draw time (large HH:MM) - centered
static void draw_time(int hours, int minutes, int seconds) {
  int display_hours = hours % 12;
  if (display_hours == 0) display_hours = 12;

  int h_tens = display_hours / 10;
  int h_ones = display_hours % 10;
  int m_tens = minutes / 10;
  int m_ones = minutes % 10;

  int colon_width = 8;

  // Calculate total width
  int total_width;
  if (h_tens > 0) {
    total_width = 4 * DIGIT_WIDTH + 2 * DIGIT_SPACING + colon_width;
  } else {
    total_width = 3 * DIGIT_WIDTH + DIGIT_SPACING + colon_width;
  }

  // Center and apply left offset correction
  int x = (DISPLAY_WIDTH - total_width) / 2 - 10;

  // Draw hour digits
  if (h_tens > 0) {
    draw_large_digit(x, TIME_Y, h_tens);
    x += DIGIT_WIDTH + DIGIT_SPACING;
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

// Draw temperature section with thermometer, temp, forecast, precipitation
static void draw_temp(void) {
  // Draw thermometer in bottom left
  draw_thermometer(8, TEMP_Y, 28);

  if (!weather_data.valid) {
    // Show placeholder when no data
    font_t font = gdispOpenFont("DejaVuSans16");
    gdispDrawString(30, TEMP_Y + 6, "--" "\xB0" "F", font, White);
    gdispCloseFont(font);
    return;
  }

  // Draw temperature with font
  char temp_str[16];
  snprintf(temp_str, sizeof(temp_str), "%d" "\xB0" "F", weather_data.temp_f);
  font_t font = gdispOpenFont("DejaVuSans16");
  gdispDrawString(30, TEMP_Y + 6, temp_str, font, White);

  // Draw condition/forecast (truncate if too long)
  char condition_short[12];
  strncpy(condition_short, weather_data.condition, sizeof(condition_short) - 1);
  condition_short[sizeof(condition_short) - 1] = '\0';

  int temp_width = gdispGetStringWidth(temp_str, font);
  gdispDrawString(30 + temp_width + 6, TEMP_Y + 6, condition_short, font, White);

  gdispCloseFont(font);

  // Draw precipitation if applicable
  bool has_precip = weather_data.precip_chance > 0 ||
                    strstr(weather_data.condition, "Rain") != NULL ||
                    strstr(weather_data.condition, "rain") != NULL ||
                    strstr(weather_data.condition, "Snow") != NULL ||
                    strstr(weather_data.condition, "snow") != NULL ||
                    strstr(weather_data.condition, "Drizzle") != NULL ||
                    strstr(weather_data.condition, "Shower") != NULL;

  if (has_precip) {
    // Draw rain icon and percentage in bottom right area
    int precip_x = DISPLAY_WIDTH - 40;
    draw_rain_icon(precip_x, TEMP_Y + 2, 12);

    if (weather_data.precip_chance > 0) {
      char precip_str[8];
      snprintf(precip_str, sizeof(precip_str), "%d%%", weather_data.precip_chance);
      font_t precip_font = gdispOpenFont("fixed_5x8");
      gdispDrawString(precip_x + 14, TEMP_Y + 10, precip_str, precip_font, White);
      gdispCloseFont(precip_font);
    }
  }
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
