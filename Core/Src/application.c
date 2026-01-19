#include "application.h"
#include <stdio.h>

static View* clock_view;
static View* flip_clock_view;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;
extern RTC_HandleTypeDef hrtc;
#define DATA_SIZE 32
uint8_t RX_Data[DATA_SIZE] = {0};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define MAX_VIEWS 1
static View* views[MAX_VIEWS];
size_t VIEW_COUNT = ARRAY_SIZE(views);
size_t CURRENT_VIEW = 0;

// Move forward with wrap-around
View* next_view(View* const views[], size_t count, size_t* index) {
  *index = (*index + 1) % count;
  return views[*index];
}

// Move backward with wrap-around
View* prev_view(View* const views[], size_t count, size_t* index) {
  *index = (*index + count - 1) % count;
  return views[*index];
}
View* get_current_view(void) {
  return views[CURRENT_VIEW];
}
// Configuration storage
#define MAX_SSID_LEN 64
#define MAX_PASSWORD_LEN 128
#define MAX_PROJECT_ID_LEN 128
#define MAX_EMAIL_LEN 256
#define MAX_PRIVATE_KEY_LEN 2048
#define MAX_CALENDAR_URL_LEN 256
#define MAX_API_KEY_LEN 64
#define MAX_CITY_LEN 64
#define MAX_COUNTRY_LEN 8

static char wifi_ssid[MAX_SSID_LEN];
static char wifi_password[MAX_PASSWORD_LEN];
static char project_id[MAX_PROJECT_ID_LEN];
static char client_email[MAX_EMAIL_LEN];
static char private_key[MAX_PRIVATE_KEY_LEN];
static char calendar_url[MAX_CALENDAR_URL_LEN];
static char openweather_api_key[MAX_API_KEY_LEN];
static char weather_city[MAX_CITY_LEN];
static char weather_country[MAX_COUNTRY_LEN];

// WIFI_SSID
bool wifi_ssid_validator(u8 str[]) {
  return strlen((char*)str) > 0 && strlen((char*)str) < MAX_SSID_LEN;
}
void wifi_ssid_updater(u8 str[]) {
  strncpy(wifi_ssid, (char*)str, MAX_SSID_LEN - 1);
  wifi_ssid[MAX_SSID_LEN - 1] = '\0';
}
void wifi_ssid_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "WIFI_SSID=%s", wifi_ssid);
}

// WIFI_PASSWORD
bool wifi_password_validator(u8 str[]) {
  return strlen((char*)str) < MAX_PASSWORD_LEN;
}
void wifi_password_updater(u8 str[]) {
  strncpy(wifi_password, (char*)str, MAX_PASSWORD_LEN - 1);
  wifi_password[MAX_PASSWORD_LEN - 1] = '\0';
}
void wifi_password_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "WIFI_PASSWORD=%s", wifi_password);
}

// PROJECT_ID
bool project_id_validator(u8 str[]) {
  return strlen((char*)str) > 0 && strlen((char*)str) < MAX_PROJECT_ID_LEN;
}
void project_id_updater(u8 str[]) {
  strncpy(project_id, (char*)str, MAX_PROJECT_ID_LEN - 1);
  project_id[MAX_PROJECT_ID_LEN - 1] = '\0';
}
void project_id_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "PROJECT_ID=%s", project_id);
}

// CLIENT_EMAIL
bool client_email_validator(u8 str[]) {
  return strlen((char*)str) > 0 && strlen((char*)str) < MAX_EMAIL_LEN;
}
void client_email_updater(u8 str[]) {
  strncpy(client_email, (char*)str, MAX_EMAIL_LEN - 1);
  client_email[MAX_EMAIL_LEN - 1] = '\0';
}
void client_email_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "CLIENT_EMAIL=%s", client_email);
}

// PRIVATE_KEY
bool private_key_validator(u8 str[]) {
  return strlen((char*)str) > 0 && strlen((char*)str) < MAX_PRIVATE_KEY_LEN;
}
void private_key_updater(u8 str[]) {
  strncpy(private_key, (char*)str, MAX_PRIVATE_KEY_LEN - 1);
  private_key[MAX_PRIVATE_KEY_LEN - 1] = '\0';
}
void private_key_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "PRIVATE_KEY=%s", private_key);
}

// CALENDAR_URL
bool calendar_url_validator(u8 str[]) {
  return strlen((char*)str) < MAX_CALENDAR_URL_LEN;
}
void calendar_url_updater(u8 str[]) {
  strncpy(calendar_url, (char*)str, MAX_CALENDAR_URL_LEN - 1);
  calendar_url[MAX_CALENDAR_URL_LEN - 1] = '\0';
}
void calendar_url_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "CALENDAR_URL=%s", calendar_url);
}

// OPENWEATHER_API_KEY
bool openweather_api_key_validator(u8 str[]) {
  return strlen((char*)str) < MAX_API_KEY_LEN;
}
void openweather_api_key_updater(u8 str[]) {
  strncpy(openweather_api_key, (char*)str, MAX_API_KEY_LEN - 1);
  openweather_api_key[MAX_API_KEY_LEN - 1] = '\0';
}
void openweather_api_key_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "OPENWEATHER_API_KEY=%s", openweather_api_key);
}

// WEATHER_CITY
bool weather_city_validator(u8 str[]) {
  return strlen((char*)str) < MAX_CITY_LEN;
}
void weather_city_updater(u8 str[]) {
  strncpy(weather_city, (char*)str, MAX_CITY_LEN - 1);
  weather_city[MAX_CITY_LEN - 1] = '\0';
}
void weather_city_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "WEATHER_CITY=%s", weather_city);
}

// WEATHER_COUNTRY
bool weather_country_validator(u8 str[]) {
  return strlen((char*)str) < MAX_COUNTRY_LEN;
}
void weather_country_updater(u8 str[]) {
  strncpy(weather_country, (char*)str, MAX_COUNTRY_LEN - 1);
  weather_country[MAX_COUNTRY_LEN - 1] = '\0';
}
void weather_country_printer(char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "WEATHER_COUNTRY=%s", weather_country);
}

static void config() {
  Disk.register_entry("WIFI_SSID", "", "#WiFi network name", &wifi_ssid_validator, &wifi_ssid_updater,
                      &wifi_ssid_printer);
  Disk.register_entry("WIFI_PASSWORD", "", "#WiFi password", &wifi_password_validator, &wifi_password_updater,
                      &wifi_password_printer);
  Disk.register_entry("PROJECT_ID", "", "#GCP project ID", &project_id_validator, &project_id_updater,
                      &project_id_printer);
  Disk.register_entry("CLIENT_EMAIL", "", "#Service account email", &client_email_validator, &client_email_updater,
                      &client_email_printer);
  Disk.register_entry("PRIVATE_KEY", "", "#Service account private key", &private_key_validator, &private_key_updater,
                      &private_key_printer);
  Disk.register_entry("CALENDAR_URL", "", "#Google Calendar iCal URL", &calendar_url_validator, &calendar_url_updater,
                      &calendar_url_printer);
  Disk.register_entry("OPENWEATHER_API_KEY", "", "#OpenWeather API key", &openweather_api_key_validator,
                      &openweather_api_key_updater, &openweather_api_key_printer);
  Disk.register_entry("WEATHER_CITY", "", "#Weather city name", &weather_city_validator, &weather_city_updater,
                      &weather_city_printer);
  Disk.register_entry("WEATHER_COUNTRY", "", "#Weather country code", &weather_country_validator,
                      &weather_country_updater, &weather_country_printer);
  Disk.init();
}

static bool ESP_READY = false;
// this is kinda awkward, but basically you need to wrap the callbacks for all the async
// functions so they can be properly invoked by the timer lib
// since it needs to be able to pass it's own callback as well
static void on_esp_weather_received(esp_weather_t* weather);
static void on_esp_status_received(esp_status_t* status);
static void on_esp_status_received_cb(void) {
  ESPComm.request_status(on_esp_status_received);
}
static void on_esp_status_received(esp_status_t* status) {
  if (!status->valid || !status->connected || status->gsheet_status != GSHEET_READY) {
    Timer.in(5000, on_esp_status_received_cb);
  } else {
    ESP_READY = true;
    // Request weather data now that ESP is ready
    ESPComm.request_weather(on_esp_weather_received);
  }
  app_log_debug("ESP status: valid=%d connected=%d connecting=%d rssi=%d gsheet=%d ip=%s", status->valid,
                status->connected, status->connecting, status->rssi, status->gsheet_status, status->ip_address);
}
static bool ESP_BALANCE_RECEIVED = false;
static bool ESP_BALANCE_REQUESTED = false;
static void on_esp_balance_received(esp_balance_t* balance) {
  if (balance->valid) {
    app_log_debug("Balance: %ld", (long)balance->balance);
    ESP_BALANCE_RECEIVED = true;
  } else {
    app_log_error("Unable to fetch balance!");
  }
}
static bool ESP_CALENDAR_RECEIVED = false;
static bool ESP_CALENDAR_REQUESTED = false;
static void on_esp_calendar_received(esp_calendar_t* cal) {
  if (cal->valid) {
    app_log_debug("Received %d calendar events", cal->event_count);
    ESP_CALENDAR_RECEIVED = true;
  } else {
    app_log_error("Unable to fetch calendar!");
  }
}

// Time handling - US Eastern timezone with automatic DST
// DST runs from 2nd Sunday of March to 1st Sunday of November

// Calculate day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
static uint8_t day_of_week(uint16_t year, uint8_t month, uint8_t day) {
  int y = year;
  int m = month;
  int d = day;
  if (m < 3) { m += 12; y--; }
  int dow = (d + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
  // Convert from Zeller (0=Sat, 1=Sun, ..., 6=Fri) to (0=Sun, 1=Mon, ..., 6=Sat)
  return (dow + 6) % 7;
}

// Find the nth occurrence of a weekday in a given month (1-based)
// weekday: 0=Sunday, n: 1=first, 2=second, etc.
static uint8_t nth_weekday_of_month(uint16_t year, uint8_t month, uint8_t weekday, uint8_t n) {
  uint8_t first_dow = day_of_week(year, month, 1);
  uint8_t first_occurrence = 1 + ((7 + weekday - first_dow) % 7);
  return first_occurrence + (n - 1) * 7;
}

// Check if DST is in effect for US Eastern timezone
// Input is UTC time, returns true if EDT (-4), false if EST (-5)
static bool is_dst_us_eastern(uint16_t year, uint8_t month, uint8_t day, uint8_t hour) {
  // DST starts: 2nd Sunday of March at 2:00 AM local (7:00 AM UTC)
  // DST ends: 1st Sunday of November at 2:00 AM local (6:00 AM UTC)

  uint8_t dst_start_day = nth_weekday_of_month(year, 3, 0, 2);  // 2nd Sunday of March
  uint8_t dst_end_day = nth_weekday_of_month(year, 11, 0, 1);   // 1st Sunday of November

  // Before March - not DST
  if (month < 3) return false;
  // After November - not DST
  if (month > 11) return false;
  // April through October - DST
  if (month > 3 && month < 11) return true;

  // March - check if we're past the transition
  if (month == 3) {
    if (day > dst_start_day) return true;
    if (day < dst_start_day) return false;
    // On the transition day, DST starts at 7:00 UTC (2:00 AM EST becomes 3:00 AM EDT)
    return hour >= 7;
  }

  // November - check if we're before the transition
  if (month == 11) {
    if (day < dst_end_day) return true;
    if (day > dst_end_day) return false;
    // On the transition day, DST ends at 6:00 UTC (2:00 AM EDT becomes 1:00 AM EST)
    return hour < 6;
  }

  return false;
}

static void on_esp_time_received(esp_time_t* time) {
  if (time->valid) {
    app_log_debug("Time (UTC): %04d-%02d-%02d %02d:%02d:%02d",
                  time->year, time->month, time->day,
                  time->hour, time->minute, time->second);

    // Determine timezone offset based on DST
    bool dst = is_dst_us_eastern(time->year, time->month, time->day, time->hour);
    int8_t tz_offset = dst ? -4 : -5;  // EDT = -4, EST = -5
    app_log_debug("DST: %s, offset: %d", dst ? "yes" : "no", tz_offset);

    // Apply timezone offset
    int16_t local_hour = time->hour + tz_offset;
    uint8_t local_day = time->day;
    uint8_t local_month = time->month;
    uint16_t local_year = time->year;

    // Handle day rollover
    if (local_hour < 0) {
      local_hour += 24;
      local_day--;
      if (local_day == 0) {
        local_month--;
        if (local_month == 0) {
          local_month = 12;
          local_year--;
        }
        // Days in each month (index 0 unused, 1=Jan, etc.)
        static const uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        local_day = days_in_month[local_month];
        // Leap year check for February
        if (local_month == 2 && ((local_year % 4 == 0 && local_year % 100 != 0) || (local_year % 400 == 0))) {
          local_day = 29;
        }
      }
    } else if (local_hour >= 24) {
      local_hour -= 24;
      local_day++;
      static const uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      uint8_t max_day = days_in_month[local_month];
      if (local_month == 2 && ((local_year % 4 == 0 && local_year % 100 != 0) || (local_year % 400 == 0))) {
        max_day = 29;
      }
      if (local_day > max_day) {
        local_day = 1;
        local_month++;
        if (local_month > 12) {
          local_month = 1;
          local_year++;
        }
      }
    }

    app_log_debug("Time (Local): %04d-%02d-%02d %02d:%02d:%02d",
                  local_year, local_month, local_day,
                  local_hour, time->minute, time->second);

    // Set the RTC with the received time
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // Convert 24-hour to 12-hour format (RTC is configured in 12-hour mode)
    uint8_t hour24 = (uint8_t)local_hour;
    if (hour24 == 0) {
      sTime.Hours = 12;
      sTime.TimeFormat = RTC_HOURFORMAT12_AM;
    } else if (hour24 < 12) {
      sTime.Hours = hour24;
      sTime.TimeFormat = RTC_HOURFORMAT12_AM;
    } else if (hour24 == 12) {
      sTime.Hours = 12;
      sTime.TimeFormat = RTC_HOURFORMAT12_PM;
    } else {
      sTime.Hours = hour24 - 12;
      sTime.TimeFormat = RTC_HOURFORMAT12_PM;
    }
    sTime.Minutes = time->minute;
    sTime.Seconds = time->second;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
      app_log_error("Failed to set RTC time!");
    }

    // Set date
    sDate.Year = local_year - 2000;  // RTC year is offset from 2000
    sDate.Month = local_month;
    sDate.Date = local_day;
    // Calculate day of week (Zeller's formula simplified)
    // WeekDay: 1=Monday, 7=Sunday for RTC
    int y = local_year;
    int m = local_month;
    int d = local_day;
    if (m < 3) { m += 12; y--; }
    int dow = (d + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
    // Convert from Zeller (0=Sat, 1=Sun, ..., 6=Fri) to RTC (1=Mon, ..., 7=Sun)
    static const uint8_t zeller_to_rtc[] = {RTC_WEEKDAY_SATURDAY, RTC_WEEKDAY_SUNDAY,
                                            RTC_WEEKDAY_MONDAY, RTC_WEEKDAY_TUESDAY,
                                            RTC_WEEKDAY_WEDNESDAY, RTC_WEEKDAY_THURSDAY,
                                            RTC_WEEKDAY_FRIDAY};
    sDate.WeekDay = zeller_to_rtc[dow];

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
      app_log_error("Failed to set RTC date!");
    }

    app_log_debug("RTC set successfully");
  } else {
    app_log_error("Unable to fetch time!");
  }
}

// Weather handling
static void on_esp_weather_received(esp_weather_t* weather);
static void request_weather_cb(void) {
  ESPComm.request_weather(on_esp_weather_received);
}
static void on_esp_weather_received(esp_weather_t* weather) {
  if (weather->valid) {
    app_log_debug("Weather: %d°F, %s, humidity=%d%%, precip=%d%%", weather->temp_f, weather->condition, weather->humidity, weather->precip_chance);
    // Update FlipClockView with weather data
    FlipClockView.set_weather(weather->temp_f, weather->condition, weather->precip_chance);
    // Request time from ESP8266 to sync the RTC
    ESPComm.request_time(on_esp_time_received);
  } else {
    app_log_error("Unable to fetch weather!");
  }
  // Request weather again in 10 minutes
  Timer.in(600000, request_weather_cb);
}
static void init() {
  app_log_debug("Application init");
  // backlight PWM
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_Base_Start_IT(&htim3);

  gfxInit();
  gdispGSetOrientation(gdispGetDisplay(0), GDISP_ROTATE_0);
  clock_view = ClockView.init();
  flip_clock_view = FlipClockView.init();

  Timer.init();

  ESPComm.init(&huart2);
  ESPComm.set_wifi(wifi_ssid, wifi_password);
  HAL_Delay(100);
  ESPComm.set_gcp_project(project_id);
  HAL_Delay(100);
  ESPComm.set_gcp_email(client_email);
  HAL_Delay(100);
  ESPComm.set_gcp_key(private_key);
  HAL_Delay(100);
  ESPComm.set_calendar_url(calendar_url);
  HAL_Delay(100);
  ESPComm.set_weather_api_key(openweather_api_key);
  HAL_Delay(100);
  ESPComm.set_weather_location(weather_city, weather_country);
  HAL_Delay(100);
  ESPComm.request_status(on_esp_status_received);
  HAL_Delay(100);

  //  DFPlayerMini.init();
  //  DFPlayerMini.volumeAdjustSet(30);

  //  DigitalEncoder.init(0x10);

  views[0] = clock_view;
  VIEW_COUNT = MAX_VIEWS;
}

// TODO: a "settings" screen to control volume and brightness, persist in
// eeprom, maybe alarm time?
static void run(void) {
  //  bank_view->render();
  // get_current_view()->render();
  flip_clock_view->render();
  ESPComm.process();
  Disk.process();
  // if (ESP_READY && (!ESP_BALANCE_RECEIVED || !ESP_CALENDAR_RECEIVED)) {
  //   if (!ESP_BALANCE_REQUESTED && !ESP_BALANCE_RECEIVED) {
  //     app_log_debug("Requesting balance");
  //     ESPComm.request_balance(on_esp_balance_received);
  //     ESP_BALANCE_REQUESTED = true;
  //   } else if (ESP_BALANCE_RECEIVED && !ESP_CALENDAR_REQUESTED && !ESP_CALENDAR_RECEIVED) {
  //     app_log_debug("Requesting calendar");
  //     ESPComm.request_calendar(10, on_esp_calendar_received);
  //     ESP_CALENDAR_REQUESTED = true;
  //   }
  // }
}
void _Error_Handler(char* file, int line) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
// Timer library callback should be triggered every 1ms
// Timer_Clock / ((Prescaler + 1) × (Counter_Period + 1))
// Don't forget you need to enable the interrupt in NVIC
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
  if (htim == &htim3) {
    Timer.tick();
  }
}

const struct application Application = {
    .config = config,
    .init = init,
    .run = run,
};
