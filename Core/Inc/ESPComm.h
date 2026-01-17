#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

// Response data structures
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  bool valid;
} esp_time_t;

typedef struct {
  int16_t temp_f;
  char condition[32];
  uint8_t humidity;
  bool valid;
} esp_weather_t;

typedef struct {
  char symbol[8];
  float price;
  bool valid;
} esp_stock_t;

typedef enum { GSHEET_NOT_INIT, GSHEET_AUTH_PENDING, GSHEET_READY } esp_gsheet_status_t;

typedef struct {
  bool connected;
  bool connecting;
  char ip_address[16];
  int8_t rssi;
  esp_gsheet_status_t gsheet_status;
  bool valid;
} esp_status_t;

typedef struct {
  int32_t balance;
  bool valid;
} esp_balance_t;

#define ESP_CALENDAR_MAX_EVENTS 10
#define ESP_CALENDAR_MAX_TITLE_LEN 64

typedef struct {
  char datetime[20];  // YYYY-MM-DD HH:MM
  char title[ESP_CALENDAR_MAX_TITLE_LEN];
} esp_calendar_event_t;

typedef struct {
  esp_calendar_event_t events[ESP_CALENDAR_MAX_EVENTS];
  uint8_t event_count;
  bool valid;
} esp_calendar_t;

// Callback function types
typedef void (*esp_status_callback_t)(esp_status_t* status);
typedef void (*esp_time_callback_t)(esp_time_t* time);
typedef void (*esp_weather_callback_t)(esp_weather_t* weather);
typedef void (*esp_stock_callback_t)(esp_stock_t* stock);
typedef void (*esp_balance_callback_t)(esp_balance_t* balance);
typedef void (*esp_calendar_callback_t)(esp_calendar_t* calendar);
typedef void (*esp_error_callback_t)(const char* error);

// Process incoming data (call regularly from main loop)
void esp_comm_process(void);

struct espcomm {
  void (*init)(UART_HandleTypeDef*);
  bool (*set_wifi)(const char*, const char*);
  bool (*set_gcp_project)(const char*);
  bool (*set_gcp_email)(const char*);
  bool (*set_gcp_key)(const char*);
  bool (*set_calendar_url)(const char*);
  bool (*request_time)(esp_time_callback_t);
  bool (*request_weather)(esp_weather_callback_t);
  bool (*request_stock)(const char*, esp_stock_callback_t);
  bool (*request_status)(esp_status_callback_t);
  bool (*request_balance)(esp_balance_callback_t);
  bool (*request_calendar)(uint8_t, esp_calendar_callback_t);
  void (*uart_irq_handler)(void);
  void (*process)(void);
};
extern const struct espcomm ESPComm;