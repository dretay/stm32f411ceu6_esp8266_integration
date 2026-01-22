// DMA-based version of ESP communication
// To use this instead of esp_comm.c:
// 1. Remove esp_comm.c from build
// 2. Rename this to esp_comm.c
// 3. Configure DMA in CubeMX as per ESP_README.md

#include "ESPComm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// RX buffer for DMA (circular)
#define ESP_RX_BUFFER_SIZE 512
static uint8_t esp_rx_buffer[ESP_RX_BUFFER_SIZE];
static uint16_t esp_rx_old_pos = 0;

// Message parsing buffer
#define ESP_MSG_BUFFER_SIZE 512
static char esp_msg_buffer[ESP_MSG_BUFFER_SIZE];
static bool esp_message_ready = false;

// TX buffer (large enough for private key ~1800 bytes)
#define ESP_TX_BUFFER_SIZE 2048
static char esp_tx_buffer[ESP_TX_BUFFER_SIZE];
static bool esp_tx_busy = false;

// Command queue - use smaller buffer for normal commands to save RAM
#define ESP_CMD_QUEUE_SIZE 8
#define ESP_CMD_QUEUE_ITEM_SIZE 384
static char esp_cmd_queue[ESP_CMD_QUEUE_SIZE][ESP_CMD_QUEUE_ITEM_SIZE];
static uint8_t esp_cmd_queue_head = 0;
static uint8_t esp_cmd_queue_tail = 0;
static uint8_t esp_cmd_queue_count = 0;

// UART handle
static UART_HandleTypeDef* esp_uart = NULL;

// Last received data
static esp_time_t last_time = {0};
static esp_weather_t last_weather = {0};
static esp_stock_t last_stock = {0};
static esp_status_t last_status = {0};
static esp_balance_t last_balance = {0};
static esp_calendar_t last_calendar = {0};

// Callbacks
static esp_time_callback_t time_callback = NULL;
static esp_weather_callback_t weather_callback = NULL;
static esp_stock_callback_t stock_callback = NULL;
static esp_status_callback_t status_callback = NULL;
static esp_balance_callback_t balance_callback = NULL;
static esp_calendar_callback_t calendar_callback = NULL;
static esp_error_callback_t error_callback = NULL;

// Internal functions
static bool esp_queue_command(const char* cmd);
static void esp_send_next_command(void);
static void esp_parse_response(const char* response);
static void esp_parse_time(const char* data);
static void esp_parse_weather(const char* data);
static void esp_parse_stock(const char* data);
static void esp_parse_status(const char* data);
static void esp_parse_balance(const char* data);
static void esp_parse_calendar(const char* data);
static void esp_process_dma_buffer(void);

// Public API functions
void esp_comm_init(UART_HandleTypeDef* huart) {
  esp_uart = huart;
  esp_rx_old_pos = 0;
  esp_message_ready = false;
  esp_tx_busy = false;
  esp_cmd_queue_head = 0;
  esp_cmd_queue_tail = 0;
  esp_cmd_queue_count = 0;

  // Enable UART idle line interrupt
  SET_BIT(esp_uart->Instance->CR1, USART_CR1_IDLEIE);

  // Start DMA reception in circular mode
  HAL_UART_Receive_DMA(esp_uart, esp_rx_buffer, ESP_RX_BUFFER_SIZE);
}

// Internal implementation
static bool esp_queue_command(const char* cmd) {
  size_t cmd_len = strlen(cmd);

  // Large command - wait for queue to drain, then send directly
  if (cmd_len >= ESP_CMD_QUEUE_ITEM_SIZE) {
    // Wait for queue to empty and TX to complete
    while (esp_tx_busy || esp_cmd_queue_count > 0) {
      // Busy wait - acceptable for one-time setup
    }
    // Send directly
    esp_tx_busy = true;
    strncpy(esp_tx_buffer, cmd, ESP_TX_BUFFER_SIZE - 1);
    esp_tx_buffer[ESP_TX_BUFFER_SIZE - 1] = '\0';
    HAL_UART_Transmit_DMA(esp_uart, (uint8_t*)esp_tx_buffer, strlen(esp_tx_buffer));
    return true;
  }

  // Normal command - queue it
  if (esp_cmd_queue_count >= ESP_CMD_QUEUE_SIZE) {
    return false;  // Queue full
  }

  // Copy command to queue
  strncpy(esp_cmd_queue[esp_cmd_queue_tail], cmd, ESP_CMD_QUEUE_ITEM_SIZE - 1);
  esp_cmd_queue[esp_cmd_queue_tail][ESP_CMD_QUEUE_ITEM_SIZE - 1] = '\0';
  esp_cmd_queue_tail = (esp_cmd_queue_tail + 1) % ESP_CMD_QUEUE_SIZE;
  esp_cmd_queue_count++;

  // If not currently transmitting, start sending
  if (!esp_tx_busy) {
    esp_send_next_command();
  }

  return true;
}

static void esp_send_next_command(void) {
  if (esp_cmd_queue_count == 0) {
    return;  // Queue empty
  }

  // Get command from head of queue
  const char* cmd = esp_cmd_queue[esp_cmd_queue_head];
  esp_cmd_queue_head = (esp_cmd_queue_head + 1) % ESP_CMD_QUEUE_SIZE;
  esp_cmd_queue_count--;

  // Send via DMA
  esp_tx_busy = true;
  strncpy(esp_tx_buffer, cmd, ESP_TX_BUFFER_SIZE - 1);
  esp_tx_buffer[ESP_TX_BUFFER_SIZE - 1] = '\0';
  HAL_UART_Transmit_DMA(esp_uart, (uint8_t*)esp_tx_buffer, strlen(esp_tx_buffer));
}

static void esp_process_dma_buffer(void) {
  // Get current DMA position
  uint16_t pos = ESP_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(esp_uart->hdmarx);

  if (pos == esp_rx_old_pos) {
    return;  // No new data
  }

  // Extract data between old_pos and pos
  static uint16_t msg_index = 0;

  if (pos > esp_rx_old_pos) {
    // Linear copy
    for (uint16_t i = esp_rx_old_pos; i < pos; i++) {
      if (esp_rx_buffer[i] == '\n') {
        // Message complete
        esp_msg_buffer[msg_index] = '\0';
        esp_message_ready = true;
        msg_index = 0;
      } else if (esp_rx_buffer[i] != '\r' && msg_index < ESP_MSG_BUFFER_SIZE - 1) {
        esp_msg_buffer[msg_index++] = esp_rx_buffer[i];
      }
    }
  } else {
    // Wrap-around copy
    for (uint16_t i = esp_rx_old_pos; i < ESP_RX_BUFFER_SIZE; i++) {
      if (esp_rx_buffer[i] == '\n') {
        esp_msg_buffer[msg_index] = '\0';
        esp_message_ready = true;
        msg_index = 0;
      } else if (esp_rx_buffer[i] != '\r' && msg_index < ESP_MSG_BUFFER_SIZE - 1) {
        esp_msg_buffer[msg_index++] = esp_rx_buffer[i];
      }
    }
    for (uint16_t i = 0; i < pos; i++) {
      if (esp_rx_buffer[i] == '\n') {
        esp_msg_buffer[msg_index] = '\0';
        esp_message_ready = true;
        msg_index = 0;
      } else if (esp_rx_buffer[i] != '\r' && msg_index < ESP_MSG_BUFFER_SIZE - 1) {
        esp_msg_buffer[msg_index++] = esp_rx_buffer[i];
      }
    }
  }

  esp_rx_old_pos = pos;
}

static void esp_parse_response(const char* response) {
  if (strncmp(response, "TIME:", 5) == 0) {
    esp_parse_time(response + 5);
  } else if (strncmp(response, "WEATHER:", 8) == 0) {
    esp_parse_weather(response + 8);
  } else if (strncmp(response, "STOCK:", 6) == 0) {
    esp_parse_stock(response + 6);
  } else if (strncmp(response, "STATUS:", 7) == 0) {
    esp_parse_status(response + 7);
  } else if (strncmp(response, "BALANCE:", 8) == 0) {
    esp_parse_balance(response + 8);
  } else if (strncmp(response, "CALENDAR:", 9) == 0) {
    esp_parse_calendar(response + 9);
  } else if (strncmp(response, "ERROR:", 6) == 0) {
    if (error_callback) {
      error_callback(response + 6);
    }
  }
  // "OK" response is acknowledged but no action needed
}

static void esp_parse_time(const char* data) {
  esp_time_t time = {0};

  int year, month, day, hour, minute, second;
  if (sscanf(data, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second) == 6) {
    time.year = year;
    time.month = month;
    time.day = day;
    time.hour = hour;
    time.minute = minute;
    time.second = second;
    time.valid = true;

    last_time = time;

    if (time_callback) {
      time_callback(&time);
    }
  } else {
    last_time.valid = false;
  }
}

static void esp_parse_weather(const char* data) {
  esp_weather_t weather = {0};

  int temp_f, temp_c, humidity, precip_chance;
  char condition[32];

  // Try new format with precip_chance first
  if (sscanf(data, "%d,%d,%31[^,],%d,%d", &temp_f, &temp_c, condition, &humidity, &precip_chance) == 5) {
    weather.temp_f = temp_f;
    strncpy(weather.condition, condition, sizeof(weather.condition) - 1);
    weather.condition[sizeof(weather.condition) - 1] = '\0';
    weather.humidity = humidity;
    weather.precip_chance = (uint8_t)precip_chance;
    weather.valid = true;

    last_weather = weather;

    if (weather_callback) {
      weather_callback(&weather);
    }
  // Fall back to old format without precip_chance
  } else if (sscanf(data, "%d,%d,%31[^,],%d", &temp_f, &temp_c, condition, &humidity) == 4) {
    weather.temp_f = temp_f;
    strncpy(weather.condition, condition, sizeof(weather.condition) - 1);
    weather.condition[sizeof(weather.condition) - 1] = '\0';
    weather.humidity = humidity;
    weather.precip_chance = 0;
    weather.valid = true;

    last_weather = weather;

    if (weather_callback) {
      weather_callback(&weather);
    }
  } else {
    last_weather.valid = false;
  }
}

static void esp_parse_stock(const char* data) {
  esp_stock_t stock = {0};

  char symbol[8];
  float price;

  if (sscanf(data, "%7[^:]:%f", symbol, &price) == 2) {
    strncpy(stock.symbol, symbol, sizeof(stock.symbol) - 1);
    stock.symbol[sizeof(stock.symbol) - 1] = '\0';
    stock.price = price;
    stock.valid = true;

    last_stock = stock;

    if (stock_callback) {
      stock_callback(&stock);
    }
  } else {
    last_stock.valid = false;
  }
}

static esp_gsheet_status_t esp_parse_gsheet_status(const char* str) {
  if (strncmp(str, "GSHEET_READY", 12) == 0) {
    return GSHEET_READY;
  } else if (strncmp(str, "GSHEET_AUTH_PENDING", 19) == 0) {
    return GSHEET_AUTH_PENDING;
  }
  return GSHEET_NOT_INIT;
}

static void esp_parse_status(const char* data) {
  esp_status_t status = {0};

  if (strncmp(data, "CONNECTED,", 10) == 0) {
    status.connected = true;
    status.connecting = false;

    // Parse IP, RSSI, and GSheet status: "192.168.1.100,-50,GSHEET_READY"
    char ip[16];
    int rssi;
    char gsheet_str[24];
    int parsed = sscanf(data + 10, "%15[^,],%d,%23s", ip, &rssi, gsheet_str);
    if (parsed >= 1) {
      strncpy(status.ip_address, ip, sizeof(status.ip_address) - 1);
      status.ip_address[sizeof(status.ip_address) - 1] = '\0';
    }
    if (parsed >= 2) {
      status.rssi = (int8_t)rssi;
    }
    if (parsed >= 3) {
      status.gsheet_status = esp_parse_gsheet_status(gsheet_str);
    }
    status.valid = true;
  } else if (strncmp(data, "CONNECTING,", 11) == 0) {
    status.connected = false;
    status.connecting = true;
    status.ip_address[0] = '\0';
    status.rssi = 0;
    status.gsheet_status = esp_parse_gsheet_status(data + 11);
    status.valid = true;
  } else if (strncmp(data, "CONNECTING", 10) == 0) {
    status.connected = false;
    status.connecting = true;
    status.ip_address[0] = '\0';
    status.rssi = 0;
    status.gsheet_status = GSHEET_NOT_INIT;
    status.valid = true;
  } else if (strncmp(data, "DISCONNECTED,", 13) == 0) {
    status.connected = false;
    status.connecting = false;
    status.ip_address[0] = '\0';
    status.rssi = 0;
    status.gsheet_status = esp_parse_gsheet_status(data + 13);
    status.valid = true;
  } else if (strncmp(data, "DISCONNECTED", 12) == 0) {
    status.connected = false;
    status.connecting = false;
    status.ip_address[0] = '\0';
    status.rssi = 0;
    status.gsheet_status = GSHEET_NOT_INIT;
    status.valid = true;
  } else {
    status.valid = false;
  }

  last_status = status;

  if (status_callback && status.valid) {
    status_callback(&status);
  }
}

static void esp_parse_balance(const char* data) {
  esp_balance_t balance = {0};

  int value;
  if (sscanf(data, "%d", &value) == 1) {
    balance.balance = value;
    balance.valid = true;

    last_balance = balance;

    if (balance_callback) {
      balance_callback(&balance);
    }
  } else {
    last_balance.valid = false;
  }
}

static void esp_parse_calendar(const char* data) {
  esp_calendar_t calendar = {0};

  // Supports two formats:
  // New: "count,start|end|title;start|end|title;..." (two pipes per event)
  // Old: "count,datetime|title;datetime|title;..." (one pipe per event)
  // Also handles "0" or "NO_EVENTS" for empty calendar

  const char* comma = strchr(data, ',');
  if (!comma) {
    // Either "0" or old "NO_EVENTS" format
    if (strcmp(data, "NO_EVENTS") == 0 || strcmp(data, "0") == 0) {
      calendar.event_count = 0;
      calendar.valid = true;
      last_calendar = calendar;
      if (calendar_callback) {
        calendar_callback(&calendar);
      }
      return;
    }
    // Invalid format
    return;
  }

  // Parse events after the comma
  const char* ptr = comma + 1;
  while (calendar.event_count < ESP_CALENDAR_MAX_EVENTS && *ptr) {
    esp_calendar_event_t* event = &calendar.events[calendar.event_count];

    // Find first pipe separator
    const char* pipe1 = strchr(ptr, '|');
    if (!pipe1)
      break;

    // Find end of this event (semicolon or end of string)
    const char* semi = strchr(ptr, ';');
    const char* event_end = semi ? semi : ptr + strlen(ptr);

    // Check if there's a second pipe before event_end (new format)
    const char* pipe2 = strchr(pipe1 + 1, '|');
    bool has_end_time = (pipe2 && pipe2 < event_end);

    if (has_end_time) {
      // New format: start|end|title
      size_t start_len = pipe1 - ptr;
      if (start_len >= sizeof(event->start)) {
        start_len = sizeof(event->start) - 1;
      }
      strncpy(event->start, ptr, start_len);
      event->start[start_len] = '\0';

      size_t end_len = pipe2 - (pipe1 + 1);
      if (end_len >= sizeof(event->end)) {
        end_len = sizeof(event->end) - 1;
      }
      strncpy(event->end, pipe1 + 1, end_len);
      event->end[end_len] = '\0';

      size_t title_len = event_end - (pipe2 + 1);
      if (title_len >= ESP_CALENDAR_MAX_TITLE_LEN) {
        title_len = ESP_CALENDAR_MAX_TITLE_LEN - 1;
      }
      strncpy(event->title, pipe2 + 1, title_len);
      event->title[title_len] = '\0';
    } else {
      // Old format: datetime|title (use datetime as both start and end)
      size_t datetime_len = pipe1 - ptr;
      if (datetime_len >= sizeof(event->start)) {
        datetime_len = sizeof(event->start) - 1;
      }
      strncpy(event->start, ptr, datetime_len);
      event->start[datetime_len] = '\0';

      // Copy same time to end field
      strncpy(event->end, event->start, sizeof(event->end) - 1);
      event->end[sizeof(event->end) - 1] = '\0';

      size_t title_len = event_end - (pipe1 + 1);
      if (title_len >= ESP_CALENDAR_MAX_TITLE_LEN) {
        title_len = ESP_CALENDAR_MAX_TITLE_LEN - 1;
      }
      strncpy(event->title, pipe1 + 1, title_len);
      event->title[title_len] = '\0';
    }

    calendar.event_count++;

    // Move to next event
    if (semi) {
      ptr = semi + 1;
    } else {
      break;
    }
  }

  calendar.valid = true;
  last_calendar = calendar;

  if (calendar_callback) {
    calendar_callback(&calendar);
  }
}

//
// refactoring starts here!!!
//
static void init(UART_HandleTypeDef* huart) {
  esp_uart = huart;
  esp_rx_old_pos = 0;
  esp_message_ready = false;
  esp_tx_busy = false;
  esp_cmd_queue_head = 0;
  esp_cmd_queue_tail = 0;
  esp_cmd_queue_count = 0;

  // Enable UART idle line interrupt
  SET_BIT(esp_uart->Instance->CR1, USART_CR1_IDLEIE);

  // Start DMA reception in circular mode
  HAL_UART_Receive_DMA(esp_uart, esp_rx_buffer, ESP_RX_BUFFER_SIZE);
}
static bool set_wifi(const char* ssid, const char* password) {
  if (!ssid || !password) {
    return false;
  }
  char cmd[ESP_TX_BUFFER_SIZE];
  snprintf(cmd, ESP_TX_BUFFER_SIZE, "WIFI:%s,%s\n", ssid, password);
  return esp_queue_command(cmd);
}

static bool set_gcp_project(const char* project_id) {
  if (!project_id) {
    return false;
  }
  char cmd[ESP_TX_BUFFER_SIZE];
  snprintf(cmd, ESP_TX_BUFFER_SIZE, "GCP_PROJECT:%s\n", project_id);
  return esp_queue_command(cmd);
}

static bool set_gcp_email(const char* client_email) {
  if (!client_email) {
    return false;
  }
  char cmd[ESP_TX_BUFFER_SIZE];
  snprintf(cmd, ESP_TX_BUFFER_SIZE, "GCP_EMAIL:%s\n", client_email);
  return esp_queue_command(cmd);
}

static bool set_gcp_key(const char* private_key) {
  if (!private_key) {
    return false;
  }
  char cmd[ESP_TX_BUFFER_SIZE];
  snprintf(cmd, ESP_TX_BUFFER_SIZE, "GCP_KEY:%s\n", private_key);
  return esp_queue_command(cmd);
}

static bool set_calendar_url(const char* url) {
  if (!url) {
    return false;
  }
  char cmd[ESP_TX_BUFFER_SIZE];
  snprintf(cmd, ESP_TX_BUFFER_SIZE, "SET_CALENDAR_URL:%s\n", url);
  return esp_queue_command(cmd);
}

static bool set_weather_api_key(const char* api_key) {
  if (!api_key) {
    return false;
  }
  char cmd[ESP_TX_BUFFER_SIZE];
  snprintf(cmd, ESP_TX_BUFFER_SIZE, "SET_WEATHER_API_KEY:%s\n", api_key);
  return esp_queue_command(cmd);
}

static bool set_weather_location(const char* city, const char* country) {
  if (!city || !country) {
    return false;
  }
  char cmd[ESP_TX_BUFFER_SIZE];
  snprintf(cmd, ESP_TX_BUFFER_SIZE, "SET_WEATHER_LOCATION:%s,%s\n", city, country);
  return esp_queue_command(cmd);
}

static bool request_time(esp_time_callback_t callback) {
  if (callback) {
    time_callback = callback;
  } else {
    return false;
  }
  return esp_queue_command("TIME\n");
}

static bool request_weather(esp_weather_callback_t callback) {
  if (callback) {
    weather_callback = callback;
  } else {
    return false;
  }
  return esp_queue_command("WEATHER\n");
}

static bool request_stock(const char* symbol, esp_stock_callback_t callback) {
  if (callback) {
    stock_callback = callback;
  } else {
    return false;
  }
  if (!symbol) {
    return false;
  }
  char cmd[ESP_TX_BUFFER_SIZE];
  snprintf(cmd, ESP_TX_BUFFER_SIZE, "STOCK:%s\n", symbol);
  return esp_queue_command(cmd);
}

static bool request_status(esp_status_callback_t callback) {
  if (callback) {
    status_callback = callback;
  } else {
    return false;
  }
  return esp_queue_command("STATUS\n");
}

static bool request_balance(esp_balance_callback_t callback) {
  if (callback) {
    balance_callback = callback;
  } else {
    return false;
  }
  return esp_queue_command("BALANCE\n");
}

static bool request_calendar(uint8_t max_events, esp_calendar_callback_t callback) {
  if (callback) {
    calendar_callback = callback;
  } else {
    return false;
  }
  if (max_events == 0) {
    return esp_queue_command("CALENDAR\n");
  } else {
    char cmd[20];
    snprintf(cmd, sizeof(cmd), "CALENDAR:%d\n", max_events);
    return esp_queue_command(cmd);
  }
}

static void set_error_callback(esp_error_callback_t callback) {
  error_callback = callback;
}

void process(void) {
  // Process DMA buffer for complete messages
  esp_process_dma_buffer();

  // Parse if message ready
  if (esp_message_ready) {
    esp_parse_response(esp_msg_buffer);
    esp_message_ready = false;
  }
}

// Helper function to be called from USART2_IRQHandler in stm32f4xx_it.c
static void uart_irq_handler(void) {
  // Handle UART idle line interrupt
  if (esp_uart && READ_BIT(esp_uart->Instance->SR, USART_SR_IDLE)) {
    // Clear idle flag by reading SR then DR
    (void)esp_uart->Instance->SR;
    (void)esp_uart->Instance->DR;

    // Process any pending data
    esp_process_dma_buffer();
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
  if (huart == esp_uart) {
    esp_tx_busy = false;
    // Send next queued command if any
    esp_send_next_command();
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
  if (huart == esp_uart) {
    // Handle error - DMA should keep running
    esp_rx_old_pos = 0;
  }
}
const struct espcomm ESPComm = {
    .init = init,
    .set_wifi = set_wifi,
    .set_gcp_project = set_gcp_project,
    .set_gcp_email = set_gcp_email,
    .set_gcp_key = set_gcp_key,
    .set_calendar_url = set_calendar_url,
    .set_weather_api_key = set_weather_api_key,
    .set_weather_location = set_weather_location,
    .request_time = request_time,
    .request_weather = request_weather,
    .request_stock = request_stock,
    .request_status = request_status,
    .request_balance = request_balance,
    .request_calendar = request_calendar,
    .set_error_callback = set_error_callback,
    .uart_irq_handler = uart_irq_handler,
    .process = process,
};
