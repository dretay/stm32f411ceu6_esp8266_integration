#pragma once

#if defined(STM32F103xB)
#include "stm32f1xx_hal.h"
#elif defined(STM32F411xE)
#include "stm32f4xx_hal.h"
#endif

#include "ClockView.h"
#include "gfx.h"

#include "disk.h"
#include "esp_comm.h"
#include "main.h"

struct application {
  void (*config)(void);
  void (*init)(void);
  void (*run)(void);
};

extern const struct application Application;

// Config value getters (values loaded from CONFIG.TXT on boot)
const char* get_wifi_ssid(void);
const char* get_wifi_password(void);
const char* get_project_id(void);
const char* get_client_email(void);
const char* get_private_key(void);
const char* get_calendar_url(void);
