#include "application.h"
#include <stdio.h>
#include "DateHelper.h"

static View* clock_view;
static View* flip_clock_view;
static View* status_view;
static View* alarm_view;
static View* calendar_view;
static View* bank_view;
static bool boot_complete = false;
struct DigitalEncoderValue old_encoder_value;

// View cycling state (0 = flip clock, 1 = calendar, 2 = bank)
static uint8_t active_view = 0;
// AlarmView editing state
static bool alarm_view_active = false;
#define CLOCK_DISPLAY_TIME 30000           // 30 seconds on clock
#define CALENDAR_DISPLAY_TIME 10000        // 10 seconds on calendar
#define BANK_DISPLAY_TIME 10000            // 10 seconds on bank
#define CALENDAR_REFRESH_INTERVAL 3600000  // 1 hour
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
static void on_esp_time_received(esp_time_t* time);
static void on_esp_weather_received(esp_weather_t* weather);
static void on_esp_balance_received(esp_balance_t* balance);
static void on_esp_calendar_received(esp_calendar_t* cal);
static void on_esp_status_received(esp_status_t* status);
static void request_weather_and_time_cb(void);
static void on_esp_error(const char* error);

// Retry delay in milliseconds
#define ESP_RETRY_DELAY 3000
static void on_esp_status_received_cb(void) {
  ESPComm.request_status(on_esp_status_received);
}
static void retry_time_cb(void) {
  app_log_debug("Retrying time request...");
  ESPComm.request_time(on_esp_time_received);
}
static void retry_weather_cb(void) {
  app_log_debug("Retrying weather request...");
  ESPComm.request_weather(on_esp_weather_received);
}
static void retry_balance_cb(void) {
  app_log_debug("Retrying balance request...");
  ESPComm.request_balance(on_esp_balance_received);
}
static void retry_calendar_cb(void) {
  app_log_debug("Retrying calendar request...");
  ESPComm.request_calendar(4, on_esp_calendar_received);
}
static void cycle_view_cb(void) {
  active_view = (active_view + 1) % 3;  // Cycle through 0, 1, 2
  // Schedule next switch based on which view we just switched to
  if (active_view == 0) {
    // Switched to clock, show for 30 seconds
    Timer.in(CLOCK_DISPLAY_TIME, cycle_view_cb);
  } else if (active_view == 1) {
    // Switched to calendar, show for 10 seconds
    Timer.in(CALENDAR_DISPLAY_TIME, cycle_view_cb);
  } else {
    // Switched to bank, show for 10 seconds
    Timer.in(BANK_DISPLAY_TIME, cycle_view_cb);
  }
}
static void refresh_calendar_cb(void) {
  app_log_debug("Refreshing calendar...");
  ESPComm.request_calendar(4, on_esp_calendar_received);
}
static void refresh_balance_cb(void) {
  app_log_debug("Refreshing balance...");
  ESPComm.request_balance(on_esp_balance_received);
}
static void request_weather_and_time_cb(void) {
  // Request time first, then weather callback will be triggered after time
  ESPComm.request_time(on_esp_time_received);
}

// Re-send all configuration to ESP8266 (called after ESP reset)
static void send_esp_config(void) {
  app_log_debug("Re-sending ESP configuration...");
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
}
static void send_esp_config_cb(void) {
  send_esp_config();
}
static void on_esp_error(const char* error) {
  app_log_error("ESP error: %s", error);

  // ESP8266 WiFi connection failed - likely reset and lost config, re-send it
  if (strstr(error, "WIFI_CONNECT_FAILED") != NULL) {
    app_log_debug("ESP8266 WiFi failed, re-sending configuration...");
    // Reset boot state to show status view again
    boot_complete = false;
    ESP_READY = false;
    StatusView.set_wifi_state(BOOT_PHASE_IN_PROGRESS);
    StatusView.set_time_state(BOOT_PHASE_PENDING);
    StatusView.set_weather_state(BOOT_PHASE_PENDING);
    StatusView.set_balance_state(BOOT_PHASE_PENDING);
    StatusView.set_calendar_state(BOOT_PHASE_PENDING);
    // Small delay before sending config
    Timer.in(500, send_esp_config_cb);
    return;
  }

  // Determine which request failed and schedule a retry
  if (strstr(error, "NTP") != NULL) {
    Timer.in(ESP_RETRY_DELAY, retry_time_cb);
  } else if (strstr(error, "WEATHER") != NULL || (strstr(error, "JSON_PARSE") != NULL && !boot_complete)) {
    // JSON_PARSE during boot is likely weather-related
    Timer.in(ESP_RETRY_DELAY, retry_weather_cb);
  } else if (strstr(error, "BALANCE") != NULL || strstr(error, "GSHEET") != NULL) {
    Timer.in(ESP_RETRY_DELAY, retry_balance_cb);
  } else if (strstr(error, "CALENDAR") != NULL) {
    Timer.in(ESP_RETRY_DELAY, retry_calendar_cb);
  } else if (strstr(error, "HTTP_") != NULL) {
    // Generic HTTP error - retry the current boot phase
    if (!boot_complete) {
      if (StatusView.is_boot_complete()) {
        // All phases complete, nothing to retry
      } else {
        // Retry based on current phase - check which phase is in progress
        // For now, just log it - the specific error handlers above should catch most cases
        app_log_debug("HTTP error during boot, waiting for next request");
      }
    }
  }
}
static void on_esp_status_received(esp_status_t* status) {
  if (!status->valid || !status->connected || status->gsheet_status != GSHEET_READY) {
    Timer.in(5000, on_esp_status_received_cb);
  } else {
    ESP_READY = true;
    // WiFi connected, mark complete and start time phase
    StatusView.set_wifi_state(BOOT_PHASE_COMPLETE);
    StatusView.set_time_state(BOOT_PHASE_IN_PROGRESS);
    // Request time first, then weather (to match boot status display order)
    ESPComm.request_time(on_esp_time_received);
  }
  app_log_debug("ESP status: valid=%d connected=%d connecting=%d rssi=%d gsheet=%d ip=%s", status->valid,
                status->connected, status->connecting, status->rssi, status->gsheet_status, status->ip_address);
}
static void on_esp_balance_received(esp_balance_t* balance) {
  if (balance->valid) {
    app_log_debug("Balance: %ld", (long)balance->balance);
    BankView.set_balance(balance->balance);
  } else {
    app_log_error("Unable to fetch balance!");
  }
  // Balance phase complete, start calendar phase
  if (!boot_complete) {
    StatusView.set_balance_state(BOOT_PHASE_COMPLETE);
    StatusView.set_calendar_state(BOOT_PHASE_IN_PROGRESS);
    ESPComm.request_calendar(4, on_esp_calendar_received);
  }
}
static void on_esp_calendar_received(esp_calendar_t* cal) {
  if (cal->valid) {
    app_log_debug("Received %d calendar events", cal->event_count);
    // Update CalendarView with the events
    CalendarView.set_events((calendar_event_t*)cal->events, cal->event_count);
  } else {
    app_log_error("Unable to fetch calendar!");
  }
  // Calendar phase complete, boot is done
  if (!boot_complete) {
    StatusView.set_calendar_state(BOOT_PHASE_COMPLETE);
    boot_complete = true;
    app_log_debug("Boot complete, switching to flip clock view");
    // Start periodic weather/time refresh (10 minutes)
    Timer.every(600000, request_weather_and_time_cb);
    // Start periodic balance refresh (10 minutes 30 seconds for spacing)
    Timer.every(600000 + 30000, refresh_balance_cb);
    // Start view cycling (clock shows first for 30 seconds, uses self-rescheduling for variable intervals)
    Timer.in(CLOCK_DISPLAY_TIME, cycle_view_cb);
    // Start calendar refresh (1 hour)
    Timer.every(CALENDAR_REFRESH_INTERVAL, refresh_calendar_cb);
  }
}

static void on_esp_time_received(esp_time_t* time) {
  if (time->valid) {
    app_log_debug("Time (UTC): %04d-%02d-%02d %02d:%02d:%02d", time->year, time->month, time->day, time->hour,
                  time->minute, time->second);

    // Apply Eastern timezone offset (handles DST automatically)
    uint16_t local_year = time->year;
    uint8_t local_month = time->month;
    uint8_t local_day = time->day;
    uint8_t local_hour = time->hour;
    DateHelper.apply_tz_offset_eastern(&local_year, &local_month, &local_day, &local_hour);

    app_log_debug("Time (Local): %04d-%02d-%02d %02d:%02d:%02d", local_year, local_month, local_day, local_hour,
                  time->minute, time->second);

    // Set the RTC with the received time
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // Convert 24-hour to 12-hour format (RTC is configured in 12-hour mode)
    if (local_hour == 0) {
      sTime.Hours = 12;
      sTime.TimeFormat = RTC_HOURFORMAT12_AM;
    } else if (local_hour < 12) {
      sTime.Hours = local_hour;
      sTime.TimeFormat = RTC_HOURFORMAT12_AM;
    } else if (local_hour == 12) {
      sTime.Hours = 12;
      sTime.TimeFormat = RTC_HOURFORMAT12_PM;
    } else {
      sTime.Hours = local_hour - 12;
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
    sDate.WeekDay = DateHelper.calc_rtc_weekday(local_year, local_month, local_day);

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
      app_log_error("Failed to set RTC date!");
    }

    app_log_debug("RTC set successfully");
    // Time received, mark complete and start weather phase
    if (!boot_complete) {
      StatusView.set_time_state(BOOT_PHASE_COMPLETE);
      StatusView.set_weather_state(BOOT_PHASE_IN_PROGRESS);
    }
    // Always request weather after time sync
    ESPComm.request_weather(on_esp_weather_received);
  } else {
    app_log_error("Unable to fetch time!");
  }
}

// Weather handling
static void on_esp_weather_received(esp_weather_t* weather);
static void on_esp_weather_received(esp_weather_t* weather) {
  if (weather->valid) {
    app_log_debug("Weather: %d°F, %s, humidity=%d%%, precip=%d%%", weather->temp_f, weather->condition,
                  weather->humidity, weather->precip_chance);
    // Update FlipClockView with weather data
    FlipClockView.set_weather(weather->temp_f, weather->condition, weather->precip_chance);
    // Weather received, mark complete and start balance phase
    if (!boot_complete) {
      StatusView.set_weather_state(BOOT_PHASE_COMPLETE);
      StatusView.set_balance_state(BOOT_PHASE_IN_PROGRESS);
      ESPComm.request_balance(on_esp_balance_received);
    }
  } else {
    app_log_error("Unable to fetch weather!");
  }
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
  status_view = StatusView.init();
  alarm_view = AlarmView.init();
  calendar_view = CalendarView.init();
  bank_view = BankView.init();

  Timer.init();
  DigitalEncoder.init(0x10);

  // Show status view and start wifi connection phase
  StatusView.set_wifi_state(BOOT_PHASE_IN_PROGRESS);

  ESPComm.init(&huart2);
  ESPComm.set_error_callback(on_esp_error);
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
  // Show status view during boot, alarm view if active, or cycle between flip clock, calendar, and bank
  if (boot_complete) {
    if (alarm_view_active) {
      alarm_view->render();
    } else if (active_view == 0) {
      flip_clock_view->render();
    } else if (active_view == 1) {
      calendar_view->render();
    } else {
      bank_view->render();
    }
  } else {
    status_view->render();
  }
  if (DigitalEncoder.irq_raised()) {
    struct DigitalEncoderValue encoder_value = DigitalEncoder.query();

    if (encoder_value.status != ENCODER_OK) {
      app_log_error("encoder error");
    }

    // Handle encoder rotation
    if (encoder_value.encoder_value > old_encoder_value.encoder_value) {
      app_log_debug("rotary encoder forward");
      if (alarm_view_active) {
        // Adjust selected field value up
        AlarmView.adjust_selected(1);
      } else {
        // Switch to alarm view
        alarm_view_active = true;
      }
    } else if (encoder_value.encoder_value < old_encoder_value.encoder_value) {
      app_log_debug("rotary encoder backward");
      if (alarm_view_active) {
        // Adjust selected field value down
        AlarmView.adjust_selected(-1);
      } else {
        // Switch to alarm view
        alarm_view_active = true;
      }
    }

    // Handle button press
    if (encoder_value.button_pressed) {
      app_log_debug("rotary encoder button");
      if (alarm_view_active) {
        // Check if we're on the last field
        if (AlarmView.get_selected_field() == ALARM_FIELD_ENABLED) {
          // Exit alarm view and return to normal view cycle
          alarm_view_active = false;
          AlarmView.set_selected_field(ALARM_FIELD_HOUR);  // Reset for next time
        } else {
          // Cycle to next field in alarm view
          AlarmView.next_field();
        }
      }
    }

    old_encoder_value = encoder_value;
  }
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
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_5) {
    DigitalEncoder.set_irq_raised(true);
  }
}

const struct application Application = {
    .config = config,
    .init = init,
    .run = run,
};
