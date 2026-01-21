#include "DateHelper.h"
#include <stdio.h>

static char WEEKDAY_MAP[][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static char MONTH_MAP[][6] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sept", "Oct", "Nov", "Dec"};

static RTC_TimeTypeDef get_time() {
  RTC_TimeTypeDef currentTime;
  HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BIN);
  return currentTime;
}
static RTC_DateTypeDef get_date() {
  RTC_DateTypeDef currentDate;
  HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BIN);
  return currentDate;
}
static time_t get_epoch() {
  /* Code to get timestamp
   *
   *  You must call HAL_RTC_GetDate() after HAL_RTC_GetTime() to unlock the
   * values in the higher-order calendar shadow registers to ensure consistency
   * between the time and date values. Reading RTC current time locks the values
   * in calendar shadow registers until Current date is read to ensure
   * consistency between the time and date values.
   */

  RTC_TimeTypeDef currentTime = get_time();
  RTC_DateTypeDef currentDate = get_date();
  struct tm currTime;

  currTime.tm_year = currentDate.Year + 100;  // In fact: 2000 + 18 - 1900
  currTime.tm_mday = currentDate.Date;
  currTime.tm_mon = currentDate.Month - 1;

  //	currTime.tm_hour = currentTime.Hours + currentTime.TimeFormat ==
  // RTC_HOURFORMAT12_PM ? 12 : 0;
  currTime.tm_min = currentTime.Minutes;
  currTime.tm_sec = currentTime.Seconds;

  return mktime(&currTime);
}
static char* get_day_of_week() {
  RTC_DateTypeDef sdatestructureget = get_date();
  return WEEKDAY_MAP[sdatestructureget.WeekDay];
}
static char* get_month() {
  RTC_DateTypeDef sdatestructureget = get_date();
  return MONTH_MAP[sdatestructureget.Month];
}
static int get_year() {
  RTC_DateTypeDef sdatestructureget = get_date();
  return sdatestructureget.Year + 2000;
}
static int minutes_since_midnight() {
  RTC_TimeTypeDef currentTime;
  RTC_DateTypeDef currentDate;
  HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BIN);

  int showHours = currentTime.Hours;
  int showMinutes = currentTime.Minutes;
  if (showHours == 0 && showMinutes == 0) {
    return 1440;
  }
  return ((showHours * 60) + showMinutes);
}
static void to_string(char* buffer) {
  RTC_TimeTypeDef currentTime;
  RTC_DateTypeDef currentDate;
  HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BIN);

  int showHours = currentTime.Hours;
  if (showHours == 0) {
    showHours = 12;
  } else if (showHours > 12) {
    showHours -= 12;
  }
  snprintf(buffer, 20, "%d:%02d %s", showHours, currentTime.Minutes, currentTime.Hours >= 12 ? "PM" : "AM");
}

// Calculate day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
static uint8_t calc_day_of_week(uint16_t year, uint8_t month, uint8_t day) {
  int y = year;
  int m = month;
  int d = day;
  if (m < 3) { m += 12; y--; }
  int dow = (d + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
  // Convert from Zeller (0=Sat, 1=Sun, ..., 6=Fri) to (0=Sun, 1=Mon, ..., 6=Sat)
  return (dow + 6) % 7;
}

// Find the nth occurrence of a weekday in a given month (1-based)
// weekday: 0=Sunday, n: 1=first, 2=second, etc.
static uint8_t nth_weekday_of_month(uint16_t year, uint8_t month, uint8_t weekday, uint8_t n) {
  uint8_t first_dow = calc_day_of_week(year, month, 1);
  uint8_t first_occurrence = 1 + ((7 + weekday - first_dow) % 7);
  return first_occurrence + (n - 1) * 7;
}

// Get days in a month, accounting for leap years
static uint8_t days_in_month(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
    return 29;
  }
  return days[month];
}

// Check if DST is in effect for US Eastern timezone
// Input is UTC time, returns true if EDT (-4), false if EST (-5)
static bool is_dst_us_eastern(uint16_t year, uint8_t month, uint8_t day, uint8_t hour) {
  // DST starts: 2nd Sunday of March at 2:00 AM local (7:00 AM UTC)
  // DST ends: 1st Sunday of November at 2:00 AM local (6:00 AM UTC)

  uint8_t dst_start_day = nth_weekday_of_month(year, 3, 0, 2);  // 2nd Sunday of March
  uint8_t dst_end_day = nth_weekday_of_month(year, 11, 0, 1);   // 1st Sunday of November

  // Before March - not DST
  if (month < 3) return false;
  // After November - not DST
  if (month > 11) return false;
  // April through October - DST
  if (month > 3 && month < 11) return true;

  // March - check if we're past the transition
  if (month == 3) {
    if (day > dst_start_day) return true;
    if (day < dst_start_day) return false;
    // On the transition day, DST starts at 7:00 UTC (2:00 AM EST becomes 3:00 AM EDT)
    return hour >= 7;
  }

  // November - check if we're before the transition
  if (month == 11) {
    if (day < dst_end_day) return true;
    if (day > dst_end_day) return false;
    // On the transition day, DST ends at 6:00 UTC (2:00 AM EDT becomes 1:00 AM EST)
    return hour < 6;
  }

  return false;
}

// Apply US Eastern timezone offset to UTC time (modifies in place)
// Handles day/month/year rollover automatically
static void apply_tz_offset_eastern(uint16_t* year, uint8_t* month, uint8_t* day, uint8_t* hour) {
  bool dst = is_dst_us_eastern(*year, *month, *day, *hour);
  int8_t tz_offset = dst ? -4 : -5;  // EDT = -4, EST = -5

  int16_t local_hour = *hour + tz_offset;

  if (local_hour < 0) {
    local_hour += 24;
    (*day)--;
    if (*day == 0) {
      (*month)--;
      if (*month == 0) {
        *month = 12;
        (*year)--;
      }
      *day = days_in_month(*year, *month);
    }
  } else if (local_hour >= 24) {
    local_hour -= 24;
    (*day)++;
    if (*day > days_in_month(*year, *month)) {
      *day = 1;
      (*month)++;
      if (*month > 12) {
        *month = 1;
        (*year)++;
      }
    }
  }

  *hour = (uint8_t)local_hour;
}

// Calculate day of week in RTC format (1=Monday, 2=Tuesday, ..., 7=Sunday)
static uint8_t calc_rtc_weekday(uint16_t year, uint8_t month, uint8_t day) {
  uint8_t dow = calc_day_of_week(year, month, day);  // 0=Sunday, 1=Monday, ..., 6=Saturday
  // Convert to RTC format: 1=Monday, ..., 7=Sunday
  return (dow == 0) ? 7 : dow;
}

const struct datehelper DateHelper = {.get_epoch = get_epoch,
                                      .get_day_of_week = get_day_of_week,
                                      .get_month = get_month,
                                      .get_year = get_year,
                                      .to_string = to_string,
                                      .minutes_since_midnight = minutes_since_midnight,
                                      .calc_day_of_week = calc_day_of_week,
                                      .nth_weekday_of_month = nth_weekday_of_month,
                                      .is_dst_us_eastern = is_dst_us_eastern,
                                      .apply_tz_offset_eastern = apply_tz_offset_eastern,
                                      .calc_rtc_weekday = calc_rtc_weekday};
