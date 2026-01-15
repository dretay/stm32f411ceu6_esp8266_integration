# Passing WiFi Credentials from STM32 to ESP8266

The ESP8266 firmware now supports receiving WiFi credentials from the STM32 and storing them in EEPROM.

## ESP8266 Side (Already Updated)

The sketch now:
- ✅ Stores WiFi credentials in EEPROM
- ✅ Uses stored credentials on boot
- ✅ Falls back to default if EEPROM empty
- ✅ Accepts new credentials via UART

### New Commands:

**Set WiFi Credentials:**
```
STM32 sends: WIFI:MyNetwork,MyPassword\n
ESP responds: OK\n  (or ERROR:...)
```

**Check Status:**
```
STM32 sends: STATUS\n
ESP responds: STATUS:CONNECTED,192.168.1.100,-65\n
             (status, IP, signal strength in dBm)
```

## STM32 Side - Usage Examples

### Example 1: Set WiFi Credentials on First Boot

```c
#include "esp_comm.h"

void app_init(void) {
  // Initialize ESP communication
  EspComm.init(&huart2);

  // Set WiFi credentials (saved to ESP EEPROM)
  if (EspComm.set_wifi("MyWiFiNetwork", "MyPassword")) {
    // Command sent successfully
    // ESP will respond with "OK" or "ERROR:..."
  }
}

void app_loop(void) {
  // Process responses
  EspComm.process();

  // Check if WiFi set command succeeded
  // (ESP will send OK or ERROR response)
}
```

### Example 2: Store Credentials in STM32 Flash

If you want to store WiFi credentials in STM32 flash and send them to ESP on boot:

```c
// Store in STM32 flash (example using defines)
#define WIFI_SSID "MyNetwork"
#define WIFI_PASSWORD "MyPassword123"

void configure_esp_wifi(void) {
  static bool configured = false;

  if (!configured) {
    // Send credentials once on boot
    if (EspComm.set_wifi(WIFI_SSID, WIFI_PASSWORD)) {
      configured = true;
    }
  }
}
```

### Example 3: Dynamic Configuration via User Interface

```c
// User enters WiFi credentials via buttons/display
void user_configure_wifi(const char* ssid, const char* password) {
  if (EspComm.is_ready()) {
    if (EspComm.set_wifi(ssid, password)) {
      // Wait for response
      // Display "Connecting..." on screen
    }
  }
}

// In your main loop
void check_wifi_status(void) {
  static uint32_t last_check = 0;

  if (HAL_GetTick() - last_check > 5000) {  // Every 5 seconds
    EspComm.request_status();
    last_check = HAL_GetTick();
  }

  // Get status
  const esp_status_t* status = EspComm.get_last_status();
  if (status->valid) {
    if (status->connected) {
      // Display: "WiFi: Connected"
      // Display IP: status->ip_address
      // Display signal: status->rssi (dBm)
    } else {
      // Display: "WiFi: Disconnected"
    }
  }
}
```

### Example 4: Callback-Based Status Monitoring

```c
void on_status_received(esp_status_t* status) {
  if (status->connected) {
    printf("WiFi connected! IP: %s, Signal: %d dBm\n",
           status->ip_address, status->rssi);
  } else {
    printf("WiFi disconnected\n");
  }
}

void setup_esp_monitoring(void) {
  EspComm.init(&huart2);
  EspComm.set_status_callback(on_status_received);

  // Set credentials
  EspComm.set_wifi("MyNetwork", "MyPassword");
}
```

## Protocol Details

### WIFI Command Format:
```
WIFI:SSID,PASSWORD\n
```

**Constraints:**
- SSID: Max 32 characters
- Password: Max 64 characters
- No commas allowed in SSID or password
- Credentials are stored in ESP8266 EEPROM (persist across reboots)

**Responses:**
- `OK\n` - Credentials saved and WiFi connection attempted
- `ERROR:INVALID_WIFI_FORMAT\n` - Missing comma or empty
- `ERROR:INVALID_WIFI_PARAMS\n` - SSID/password too long
- `ERROR:WIFI_CONNECT_FAILED\n` - Could not connect with credentials

### STATUS Command:
```
STATUS\n
```

**Response:**
- `STATUS:CONNECTED,192.168.1.100,-65\n` - Connected (IP, signal strength)
- `STATUS:DISCONNECTED\n` - Not connected

## STM32 Implementation

You need to add these implementations to `esp_comm.c`:

```c
// Add to globals
static esp_status_t last_status = {0};
static esp_status_callback_t status_callback = NULL;

// Set WiFi function
static bool esp_comm_set_wifi_impl(const char* ssid, const char* password) {
  if (esp_tx_busy || !ssid || !password) {
    return false;
  }

  // Format: WIFI:ssid,password\n
  int len = snprintf(esp_tx_buffer, ESP_TX_BUFFER_SIZE,
                     "WIFI:%s,%s\n", ssid, password);

  if (len >= ESP_TX_BUFFER_SIZE) {
    return false;  // Too long
  }

  esp_send_command(esp_tx_buffer);
  return true;
}

// Request status function
static bool esp_comm_request_status_impl(void) {
  if (esp_tx_busy) {
    return false;
  }
  esp_send_command("STATUS\n");
  return true;
}

// Get status function
static const esp_status_t* esp_comm_get_last_status_impl(void) {
  return &last_status;
}

// Set status callback
static void esp_comm_set_status_callback_impl(esp_status_callback_t callback) {
  status_callback = callback;
}

// Parse STATUS response
static void esp_parse_status(const char* data) {
  // Parse: CONNECTED,192.168.1.100,-65  or  DISCONNECTED
  esp_status_t status = {0};

  if (strncmp(data, "CONNECTED,", 10) == 0) {
    status.connected = true;

    // Parse IP and RSSI
    char ip[16];
    int rssi;
    if (sscanf(data + 10, "%15[^,],%d", ip, &rssi) == 2) {
      strncpy(status.ip_address, ip, sizeof(status.ip_address) - 1);
      status.rssi = rssi;
      status.valid = true;
    }
  } else if (strcmp(data, "DISCONNECTED") == 0) {
    status.connected = false;
    status.valid = true;
  }

  last_status = status;

  if (status_callback && status.valid) {
    status_callback(&status);
  }
}

// Add to esp_parse_response():
else if (strncmp(response, "STATUS:", 7) == 0) {
  esp_parse_status(response + 7);
}
else if (strcmp(response, "OK") == 0) {
  // WiFi credentials accepted
  // Could add a callback for this
}

// Update the EspComm struct:
const struct esp_comm EspComm = {
    .init = esp_comm_init_impl,
    .process = esp_comm_process_impl,
    .set_wifi = esp_comm_set_wifi_impl,
    .request_time = esp_comm_request_time_impl,
    .request_weather = esp_comm_request_weather_impl,
    .request_stock = esp_comm_request_stock_impl,
    .request_status = esp_comm_request_status_impl,
    .get_last_time = esp_comm_get_last_time_impl,
    .get_last_weather = esp_comm_get_last_weather_impl,
    .get_last_stock = esp_comm_get_last_stock_impl,
    .get_last_status = esp_comm_get_last_status_impl,
    .set_time_callback = esp_comm_set_time_callback_impl,
    .set_weather_callback = esp_comm_set_weather_callback_impl,
    .set_stock_callback = esp_comm_set_stock_callback_impl,
    .set_status_callback = esp_comm_set_status_callback_impl,
    .set_error_callback = esp_comm_set_error_callback_impl,
    .is_ready = esp_comm_is_ready_impl,
};
```

## Benefits

**Security:**
- WiFi credentials never hardcoded in ESP firmware
- Can be stored in secure STM32 flash
- Can be changed without reflashing ESP

**Flexibility:**
- Change WiFi without reprogramming ESP
- Support multiple networks
- User can configure via interface

**Convenience:**
- ESP remembers credentials in EEPROM
- Works across ESP reboots
- Falls back to defaults if EEPROM cleared

## Testing

**1. Test with Serial Monitor:**
```
# Upload firmware to ESP
# Open Serial Monitor (115200 baud)
# Type: WIFI:MyNetwork,MyPassword
# ESP responds: OK
# Type: STATUS
# ESP responds: STATUS:CONNECTED,192.168.1.100,-65
```

**2. Test from STM32:**
```c
// In your app init
EspComm.init(&huart2);
EspComm.set_wifi("TestNetwork", "TestPassword");

// In main loop
EspComm.process();

// Check after a few seconds
const esp_status_t* status = EspComm.get_last_status();
if (status->valid && status->connected) {
  // Success!
}
```

## Notes

- Credentials are stored in ESP8266 EEPROM at address 0
- EEPROM validated with magic number (0xAA55) and checksum
- Invalid EEPROM data causes fallback to defaults
- ESP auto-reconnects if WiFi drops
- Signal strength (RSSI): -30 = excellent, -67 = good, -80 = poor

## Complete Implementation

The ESP8266 firmware has been fully updated. You need to add the STM32 implementation code shown above to your `esp_comm.c` file.

Would you like me to generate the complete updated `esp_comm.c` file with all these functions implemented?
