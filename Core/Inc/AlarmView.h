#pragma once

#include "View.h"
#include <stdbool.h>
#include <stdint.h>

struct alarmview {
  View* (*init)(void);
  void (*set_alarm_hour)(uint8_t hour);
  void (*set_alarm_minute)(uint8_t minute);
  void (*set_enabled)(bool enabled);
  bool (*is_enabled)(void);
  uint8_t (*get_alarm_hour)(void);
  uint8_t (*get_alarm_minute)(void);
};
extern const struct alarmview AlarmView;
