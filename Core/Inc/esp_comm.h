#pragma once

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

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

typedef enum {
  GSHEET_NOT_INIT,
  GSHEET_AUTH_PENDING,
  GSHEET_READY
} esp_gsheet_status_t;

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

// Initialize ESP communication
void esp_comm_init(UART_HandleTypeDef* huart);

// Process incoming data (call regularly from main loop)
void esp_comm_process(void);

// Configuration functions
bool esp_comm_set_wifi(const char* ssid, const char* password);
bool esp_comm_set_gcp_project(const char* project_id);
bool esp_comm_set_gcp_email(const char* client_email);
bool esp_comm_set_gcp_key(const char* private_key);
bool esp_comm_set_calendar_url(const char* url);

// Request functions (non-blocking, returns false if busy)
bool esp_comm_request_time(void);
bool esp_comm_request_weather(void);
bool esp_comm_request_stock(const char* symbol);
bool esp_comm_request_status(void);
bool esp_comm_request_balance(void);
bool esp_comm_request_calendar(uint8_t max_events);  // 0 = default (10)

// Get last received data (for polling mode)
const esp_time_t* esp_comm_get_last_time(void);
const esp_weather_t* esp_comm_get_last_weather(void);
const esp_stock_t* esp_comm_get_last_stock(void);
const esp_status_t* esp_comm_get_last_status(void);
const esp_balance_t* esp_comm_get_last_balance(void);
const esp_calendar_t* esp_comm_get_last_calendar(void);

// Register callbacks (optional - for async operation)
void esp_comm_set_time_callback(esp_time_callback_t callback);
void esp_comm_set_weather_callback(esp_weather_callback_t callback);
void esp_comm_set_stock_callback(esp_stock_callback_t callback);
void esp_comm_set_status_callback(esp_status_callback_t callback);
void esp_comm_set_balance_callback(esp_balance_callback_t callback);
void esp_comm_set_calendar_callback(esp_calendar_callback_t callback);
void esp_comm_set_error_callback(esp_error_callback_t callback);

// Check if ESP is ready to send another command
bool esp_comm_is_ready(void);

// UART IRQ helper - call this from USART2_IRQHandler if using DMA mode
void esp_comm_uart_irq_handler(void);
