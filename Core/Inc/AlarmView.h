#pragma once

#include "View.h"
#include <stdbool.h>
#include <stdint.h>

// Editable fields in AlarmView
typedef enum {
  ALARM_FIELD_HOUR = 0,
  ALARM_FIELD_MINUTE = 1,
  ALARM_FIELD_ENABLED = 2,
  ALARM_FIELD_COUNT = 3
} alarm_field_t;

struct alarmview {
  View* (*init)(void);
  void (*set_alarm_hour)(uint8_t hour);
  void (*set_alarm_minute)(uint8_t minute);
  void (*set_enabled)(bool enabled);
  bool (*is_enabled)(void);
  uint8_t (*get_alarm_hour)(void);
  uint8_t (*get_alarm_minute)(void);
  // Field selection for editing
  void (*set_selected_field)(alarm_field_t field);
  alarm_field_t (*get_selected_field)(void);
  void (*next_field)(void);
  void (*adjust_selected)(int8_t delta);
};
extern const struct alarmview AlarmView;
