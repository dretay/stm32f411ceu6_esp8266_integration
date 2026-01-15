#ifndef HAL_COMPAT_H
#define HAL_COMPAT_H

// Override problematic macros for host compilation on macOS

// Disable RAM function attribute (not supported on macOS)
#undef __RAM_FUNC
#define __RAM_FUNC

// Include HAL mock types
#include "stm32_hal_mock.h"

#endif // HAL_COMPAT_H
