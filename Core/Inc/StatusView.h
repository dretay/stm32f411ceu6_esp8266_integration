#pragma once

#include "View.h"
#include <stdbool.h>

// Boot phase states
typedef enum {
  BOOT_PHASE_PENDING,
  BOOT_PHASE_IN_PROGRESS,
  BOOT_PHASE_COMPLETE
} boot_phase_state_t;

struct statusview {
  View* (*init)(void);
  void (*set_wifi_state)(boot_phase_state_t state);
  void (*set_time_state)(boot_phase_state_t state);
  void (*set_weather_state)(boot_phase_state_t state);
  void (*set_balance_state)(boot_phase_state_t state);
  void (*set_calendar_state)(boot_phase_state_t state);
  bool (*is_boot_complete)(void);
};
extern const struct statusview StatusView;
