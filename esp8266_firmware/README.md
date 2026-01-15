# ESP8266 Firmware for STM32 Communication

This Arduino sketch runs on the ESP8266 and provides NTP time, weather, and stock data to your STM32 via UART.

## Quick Start

### 1. Install Arduino IDE

Download from: https://www.arduino.cc/en/software

### 2. Install ESP8266 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In "Additional Board Manager URLs", add:
   ```
   http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search for "esp8266"
6. Install "**esp8266 by ESP8266 Community**"

### 3. Install Required Libraries

Go to **Tools → Manage Libraries** and install:

- **NTPClient** by Fabrice Weinberg
- **ArduinoJson** by Benoit Blanchon (version 6.x)

*(ESP8266WiFi, ESP8266HTTPClient are already included with ESP8266 board support)*

### 4. Configure the Sketch

Open `esp8266_firmware.ino` and edit:

```cpp
// WiFi credentials
const char* WIFI_SSID = "YourWiFiName";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// Weather API key (OpenWeatherMap - free)
const char* WEATHER_API_KEY = "your_key_here";
const char* WEATHER_CITY = "San Francisco";

// Stock API key (Alpha Vantage - free, 25 calls/day)
const char* STOCK_API_KEY = "your_key_here";

// Timezone offset in seconds (PST = -8 hours)
const long UTC_OFFSET_SEC = -8 * 3600;
```

### 5. Get Free API Keys

**Weather (OpenWeatherMap):**
1. Go to: https://openweathermap.org/api
2. Sign up for free account
3. Get your API key (free tier: 1000 calls/day)

**Stocks (Alpha Vantage):**
1. Go to: https://www.alphavantage.co/support/#api-key
2. Get your free API key (25 calls/day)

### 6. Upload to ESP8266

1. Connect ESP8266 via USB
2. In Arduino IDE:
   - **Tools → Board** → Select your board (e.g., "NodeMCU 1.0" or "LOLIN(WEMOS) D1 R2 & mini")
   - **Tools → Port** → Select the correct COM port
   - **Tools → Upload Speed** → 115200
3. Click **Upload** (→ button)

### 7. Test with Serial Monitor

1. Open **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. Type commands:
   - `TIME` → Should return: `TIME:2026-01-08T12:34:56Z`
   - `WEATHER` → Should return: `WEATHER:72,-22,Sunny,45`
   - `STOCK:AAPL` → Should return: `STOCK:AAPL:185.23`

### 8. Connect to STM32

**Wiring:**
```
ESP8266         STM32F411CE
-------         -----------
TX (GPIO1)  --> PA3 (USART2_RX)
RX (GPIO3)  <-- PA2 (USART2_TX)
GND         --- GND
3.3V        --- External regulator (see below)
```

**Power:**
- **⚠️ Important:** ESP8266 needs 300-400mA during WiFi transmission
- STM32's 3.3V pin provides only ~50mA
- **Use external regulator** (AMS1117-3.3V) or power ESP via USB

## Protocol Reference

### Commands (STM32 → ESP8266)
```
TIME\n          - Request current time from NTP
WEATHER\n       - Request weather data
STOCK:AAPL\n    - Request stock price for symbol AAPL
```

### Responses (ESP8266 → STM32)
```
TIME:2026-01-08T12:34:56Z\n
WEATHER:72,-22,Sunny,45\n         (temp_f, temp_c, condition, humidity)
STOCK:AAPL:185.23\n
ERROR:message\n
```

## Customization

### Change Update Intervals

```cpp
// In the sketch:
const unsigned long WEATHER_CACHE_TIME = 900000;  // 15 minutes (in ms)
const unsigned long STOCK_CACHE_TIME = 60000;     // 1 minute
```

### Add More Cities

Modify the `WEATHER_CITY` and `WEATHER_COUNTRY_CODE`:
```cpp
const char* WEATHER_CITY = "London";
const char* WEATHER_COUNTRY_CODE = "GB";
```

### Support Multiple Commands

You can extend `processCommand()` to handle more requests:

```cpp
void processCommand(String cmd) {
  cmd.trim();

  if (cmd == "TIME") {
    handleTimeRequest();
  }
  else if (cmd == "WEATHER") {
    handleWeatherRequest();
  }
  else if (cmd.startsWith("WEATHER:")) {
    String city = cmd.substring(8);
    handleWeatherRequest(city);  // Custom city
  }
  // Add more commands here...
}
```

## Troubleshooting

### ESP8266 Won't Connect to WiFi
- Check SSID and password are correct
- Make sure WiFi is 2.4GHz (ESP8266 doesn't support 5GHz)
- Check signal strength (move closer to router)

### "ERROR:HTTP_401" (Unauthorized)
- Check API keys are correct
- Make sure API key is activated (can take a few minutes)

### "ERROR:HTTP_429" (Too Many Requests)
- You've exceeded free tier limits
- Wait or upgrade to paid plan

### No Response from ESP8266
- Check wiring (TX→RX, RX→TX, not TX→TX)
- Check baud rate matches (115200)
- Use Serial Monitor to test ESP independently

### Stock Data Not Working
- Alpha Vantage free tier: 25 requests/day, 5 per minute
- Try again later or use a different API

### Weather Data Incorrect
- Check timezone offset: `UTC_OFFSET_SEC`
- OpenWeatherMap uses UTC by default

## Alternative APIs

### Weather Alternatives:
- **WeatherAPI.com** - 1M calls/month free
- **OpenMeteo** - Free, no API key needed
- **WeatherStack** - 1000 calls/month free

### Stock Alternatives:
- **Finnhub** - 60 calls/minute free
- **IEX Cloud** - 50k calls/month free
- **Yahoo Finance** (unofficial, no key needed)

To switch APIs, modify the `handleWeatherRequest()` or `handleStockRequest()` functions.

## Performance

- **Boot time:** ~2-3 seconds
- **NTP update:** ~200ms
- **Weather API:** ~500-1000ms
- **Stock API:** ~500-1000ms
- **Idle power:** ~70mA
- **Active WiFi:** ~150-300mA peak

## Board Compatibility

Tested on:
- ✅ NodeMCU v1.0
- ✅ Wemos D1 Mini
- ✅ ESP-01 (needs USB adapter for programming)
- ✅ ESP-12F

Should work on any ESP8266 module with UART0 (GPIO1/GPIO3).

## License

Free to use and modify for your projects.

## Support

For ESP8266-specific issues, check:
- ESP8266 Arduino Core: https://github.com/esp8266/Arduino
- Community Forum: https://www.esp8266.com/
