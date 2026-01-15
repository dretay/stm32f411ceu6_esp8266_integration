#pragma once

#if defined(STM32F103xB)
#include "stm32f1xx_hal.h"
#elif defined(STM32F411xE)
#include "stm32f4xx_hal.h"
#endif

#include "gfx.h"

typedef struct {
  void (*render)(void);
} View;
