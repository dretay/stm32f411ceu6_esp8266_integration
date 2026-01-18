#pragma once

#include "View.h"
#include <stdbool.h>

// Weather data for display
typedef struct {
  int16_t temp_f;
  char condition[32];
  uint8_t precip_chance;  // 0-100 percentage
  bool valid;
} flipclock_weather_t;

struct flipclockview {
  View* (*init)(void);
  void (*set_weather)(int16_t temp_f, const char* condition, uint8_t precip_chance);
};
extern const struct flipclockview FlipClockView;
