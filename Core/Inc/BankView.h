#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "View.h"

struct bankview {
  View* (*init)(void);
  void (*set_balance)(int32_t balance);
};
extern const struct bankview BankView;
