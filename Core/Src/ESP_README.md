# ESP8266 Communication Module

This module provides non-blocking UART communication between STM32F411 and ESP8266 for fetching NTP time, weather, and stock data.

## Hardware Setup

### Connections
```
STM32F411CE         ESP8266
-----------         -------
PA2 (USART2_TX) --> RX
PA3 (USART2_RX) <-- TX
GND             --- GND
3.3V            --- VCC (ensure adequate current, ~300mA peak)
```

**Important:**
- Use a 3.3V supply capable of 300-500mA for ESP8266
- STM32 3.3V pin may not provide enough current - consider external regulator
- Add 10µF capacitor near ESP VCC pin to prevent brownouts

### STM32CubeMX Configuration

1. **Enable USART2:**
   - Mode: Asynchronous
   - Baud Rate: 115200
   - Word Length: 8 Bits
   - Stop Bits: 1
   - Parity: None
   - Hardware Flow Control: None

2. **Enable USART2 Interrupts:**
   - NVIC Settings → USART2 global interrupt: Enabled
   - Priority: 5 (or appropriate for your application)

3. **GPIO Configuration:**
   - PA2: USART2_TX (already configured)
   - PA3: USART2_RX (already configured)

## Software Integration

### 1. Add Files to Build

Add to `CMakeLists.txt`:
```cmake
# Add ESP communication module
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/esp_comm.c
)
```

### 2. Initialize in Your Application

```c
#include "esp_comm.h"
#include "main.h"  // For huart2

// In your init function
void app_init(void) {
    // Initialize ESP communication
    esp_comm_init(&huart2);

    // Optional: Register callbacks for async operation
    esp_set_time_callback(on_time_received);
    esp_set_weather_callback(on_weather_received);
    esp_set_stock_callback(on_stock_received);
    esp_set_error_callback(on_error);
}
```

### 3. Process in Main Loop

```c
void app_run(void) {
    // Process incoming ESP responses
    esp_process();

    // Request data as needed
    static uint32_t last_update = 0;
    if (HAL_GetTick() - last_update > 60000) {  // Every 60 seconds
        if (esp_is_ready()) {
            esp_request_time();
            last_update = HAL_GetTick();
        }
    }

    // Your other application code...
}
```

## Protocol Specification

### Commands (STM32 → ESP8266)
```
TIME\n          - Request current time from NTP
WEATHER\n       - Request weather data
STOCK:AAPL\n    - Request stock price for symbol AAPL
```

### Responses (ESP8266 → STM32)
```
TIME:2026-01-08T12:34:56Z\n
WEATHER:72,-22,Sunny,45\n         (temp_f,temp_c,condition,humidity)
STOCK:AAPL:185.23\n
ERROR:NO_WIFI\n
ERROR:HTTP_FAILED\n
```

## API Reference

### Initialization
```c
void esp_comm_init(UART_HandleTypeDef* huart);
```
Initialize the ESP communication module. Call once during startup.

### Request Functions (Non-blocking)
```c
bool esp_request_time(void);
bool esp_request_weather(void);
bool esp_request_stock(const char* symbol);
```
Returns `false` if ESP is busy, `true` if request was sent.

### Callback Registration (Optional)
```c
void esp_set_time_callback(esp_time_callback_t callback);
void esp_set_weather_callback(esp_weather_callback_t callback);
void esp_set_stock_callback(esp_stock_callback_t callback);
void esp_set_error_callback(esp_error_callback_t callback);
```

### Polling Functions
```c
const esp_time_t* esp_get_last_time(void);
const esp_weather_t* esp_get_last_weather(void);
const esp_stock_t* esp_get_last_stock(void);
```
Get the last received data. Check `.valid` field before using.

### Process Function
```c
void esp_process(void);
```
**Must be called regularly** from main loop to parse incoming responses.

### Status Check
```c
bool esp_is_ready(void);
```
Returns `true` if ready to send a new command.

## Usage Patterns

### Pattern 1: Polling Mode
```c
void update_time(void) {
    esp_request_time();

    // Later in main loop...
    const esp_time_t* time = esp_get_last_time();
    if (time->valid) {
        display_time(time->hour, time->minute, time->second);
    }
}
```

### Pattern 2: Callback Mode
```c
static void on_time_received(esp_time_t* time) {
    if (time->valid) {
        update_rtc(time);  // Update hardware RTC
        refresh_display();
    }
}

void init(void) {
    esp_set_time_callback(on_time_received);
}
```

### Pattern 3: State Machine (Recommended)
```c
typedef enum {
    STATE_IDLE,
    STATE_WAITING_TIME,
    STATE_WAITING_WEATHER
} app_state_t;

static app_state_t state = STATE_IDLE;
static uint32_t request_time = 0;

void app_loop(void) {
    esp_process();

    switch (state) {
        case STATE_IDLE:
            if (esp_is_ready()) {
                esp_request_time();
                state = STATE_WAITING_TIME;
                request_time = HAL_GetTick();
            }
            break;

        case STATE_WAITING_TIME: {
            const esp_time_t* time = esp_get_last_time();
            if (time->valid) {
                // Got time, now get weather
                esp_request_weather();
                state = STATE_WAITING_WEATHER;
                request_time = HAL_GetTick();
            }
            // Timeout after 5 seconds
            if (HAL_GetTick() - request_time > 5000) {
                state = STATE_IDLE;
            }
            break;
        }

        case STATE_WAITING_WEATHER: {
            const esp_weather_t* weather = esp_get_last_weather();
            if (weather->valid) {
                display_weather(weather);
                state = STATE_IDLE;
            }
            if (HAL_GetTick() - request_time > 10000) {
                state = STATE_IDLE;
            }
            break;
        }
    }
}
```

## Update Intervals

Recommended update intervals:
- **NTP Time**: Every 15-60 minutes (time drift is minimal)
- **Weather**: Every 15-30 minutes (API rate limits apply)
- **Stocks**: Every 1-5 minutes (during market hours only)

## Error Handling

The module handles:
- ✅ UART errors (automatically restarts receive)
- ✅ Buffer overflows (silently discards excess data)
- ✅ Malformed responses (returns invalid data)
- ✅ Timeouts (use state machine pattern)

## Debugging

### Enable UART TX to monitor commands:
```c
// In esp_comm.c, add before HAL_UART_Transmit_IT:
HAL_UART_Transmit(&huart1, (uint8_t*)cmd, strlen(cmd), 100);  // Debug UART
```

### Check if interrupts are working:
```c
// In HAL_UART_RxCpltCallback, toggle LED
HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
```

## Next Steps

1. Flash ESP8266 with Arduino sketch (see ESP8266_README.md)
2. Test with serial terminal first
3. Integrate into your application
4. Add error handling and timeouts
5. Implement periodic update logic

## Example ESP8266 Arduino Sketch

See the companion ESP8266 sketch in the `esp8266_firmware/` directory for the corresponding firmware that implements this protocol.

## Troubleshooting

**No response from ESP:**
- Check wiring (TX→RX, RX→TX)
- Verify baud rate matches (115200)
- Ensure ESP has adequate power
- Check if USART2 interrupt is enabled

**Garbled data:**
- Check baud rate on both sides
- Verify 3.3V logic levels
- Check for ground loops

**Timeout errors:**
- ESP may need more time for HTTP requests
- Increase timeout in state machine
- Check WiFi connection on ESP
