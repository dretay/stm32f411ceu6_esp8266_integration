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

const struct datehelper DateHelper = {.get_epoch = get_epoch,
                                      .get_day_of_week = get_day_of_week,
                                      .get_month = get_month,
                                      .get_year = get_year,
                                      .to_string = to_string,
                                      .minutes_since_midnight = minutes_since_midnight};
