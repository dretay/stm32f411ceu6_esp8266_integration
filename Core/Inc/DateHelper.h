#pragma once

#include "time.h"
#include <stdbool.h>
#include <stdint.h>
#if defined(STM32F103xB)
#include "stm32f1xx_hal.h"
#elif defined(STM32F411xE)
#include "stm32f4xx_hal.h"
#endif
extern RTC_HandleTypeDef hrtc;

struct datehelper {
  time_t (*get_epoch)(void);
  char* (*get_day_of_week)(void);
  char* (*get_month)();
  int (*get_year)();
  void (*to_string)(char* buffer);
  int (*minutes_since_midnight)(void);
  uint8_t (*calc_day_of_week)(uint16_t year, uint8_t month, uint8_t day);
  uint8_t (*nth_weekday_of_month)(uint16_t year, uint8_t month, uint8_t weekday, uint8_t n);
  bool (*is_dst_us_eastern)(uint16_t year, uint8_t month, uint8_t day, uint8_t hour);
  void (*apply_tz_offset_eastern)(uint16_t* year, uint8_t* month, uint8_t* day, uint8_t* hour);
  uint8_t (*calc_rtc_weekday)(uint16_t year, uint8_t month, uint8_t day);
};
extern const struct datehelper DateHelper;
