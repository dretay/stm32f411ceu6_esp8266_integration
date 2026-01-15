#pragma once

#include "time.h"
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
};
extern const struct datehelper DateHelper;
