#include "application.h"
#include <stdio.h>

static View* clock_view;
extern TIM_HandleTypeDef htim1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;
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

static char wifi_ssid[MAX_SSID_LEN];
static char wifi_password[MAX_PASSWORD_LEN];
static char project_id[MAX_PROJECT_ID_LEN];
static char client_email[MAX_EMAIL_LEN];
static char private_key[MAX_PRIVATE_KEY_LEN];
static char calendar_url[MAX_CALENDAR_URL_LEN];

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

// Getters for use by other modules
const char* get_wifi_ssid(void) {
  return wifi_ssid;
}
const char* get_wifi_password(void) {
  return wifi_password;
}
const char* get_project_id(void) {
  return project_id;
}
const char* get_client_email(void) {
  return client_email;
}
const char* get_private_key(void) {
  return private_key;
}
const char* get_calendar_url(void) {
  return calendar_url;
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
  Disk.init();
}
static void init() {
  app_log_debug("Application init");
  // backlight PWM
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

  // hold esp in reset
  //  HAL_GPIO_WritePin(ESP_RST_GPIO_Port, ESP_RST_Pin, GPIO_PIN_RESET);

  gfxInit();
  gdispGSetOrientation(gdispGetDisplay(0), GDISP_ROTATE_0);

  esp_comm_init(&huart2);
  // Set WiFi credentials (saves to ESP EEPROM)
  esp_comm_set_wifi(wifi_ssid, wifi_password);

  // Set GCP credentials
  esp_comm_set_gcp_project(project_id);
  esp_comm_set_gcp_email(client_email);
  esp_comm_set_gcp_key(private_key);

  // Set calendar URL if configured
  if (strlen(calendar_url) > 0) {
    esp_comm_set_calendar_url(calendar_url);
  }

  // Check connection status
  esp_comm_request_status();

  clock_view = ClockView.init();

  //  DFPlayerMini.init();
  //  DFPlayerMini.volumeAdjustSet(30);

  //  DigitalEncoder.init(0x10);

  views[0] = clock_view;
  VIEW_COUNT = MAX_VIEWS;

  // wait a sec after serial comms enabled for things to settle before booting
  // esp
  //   SerialCommand.init();
  app_log_debug("Wifi SSID: %s", get_wifi_ssid());
}

// Balance request state
static bool balance_requested = false;
static bool balance_logged = false;

// Status polling
static uint32_t last_status_request = 0;
#define STATUS_POLL_INTERVAL_MS 5000

// TODO: a "settings" screen to control volume and brightness, persist in
// eeprom, maybe alarm time?
static void run(void) {
  //   SerialCommand.process();
  //  bank_view->render();
  get_current_view()->render();
  // In loop
  esp_comm_process();
  Disk.process();  // Flush deferred USB mass storage writes to flash

  const esp_status_t* status = esp_comm_get_last_status();
  uint32_t now = HAL_GetTick();

  // Poll status periodically until connected and gsheet ready
  if (!status->valid || !status->connected || status->gsheet_status != GSHEET_READY) {
    if (now - last_status_request >= STATUS_POLL_INTERVAL_MS) {
      last_status_request = now;
      esp_comm_request_status();
    }
  } else {
    // GSheet is ready - request balance once
    if (!balance_requested) {
      app_log_debug("GSheet ready, requesting balance...");
      esp_comm_request_balance();
      balance_requested = true;
    }
  }

  // Check for balance response
  const esp_balance_t* balance = esp_comm_get_last_balance();
  if (balance->valid && !balance_logged) {
    app_log_debug("Balance: %ld", (long)balance->balance);
    balance_logged = true;

    // Request calendar events (0 = default 10 events)
    esp_comm_request_calendar(10);

    // Poll for response
    const esp_calendar_t* cal = esp_comm_get_last_calendar();
    if (cal->valid) {
      for (int i = 0; i < cal->event_count; i++) {
        app_log_debug("Event: %s - %s",
                      cal->events[i].datetime,  // "2024-01-15 10:00"
                      cal->events[i].title);    // "Meeting"
      }
    }
  }
  //   if (DigitalEncoder.irq_raised()) {
  //     struct DigitalEncoderValue encoder_value = DigitalEncoder.query();
  //     if (encoder_value.encoder_value > old_encoder_value.encoder_value) {
  //       next_view(views, VIEW_COUNT, &CURRENT_VIEW);
  //     } else if (encoder_value.encoder_value <
  //     old_encoder_value.encoder_value) {
  //       prev_view(views, VIEW_COUNT, &CURRENT_VIEW);
  //     } else if (encoder_value.button_value == true) {
  //       int songToPlay = DateHelper.minutes_since_midnight();
  //       DFPlayerMini.playFromMP3Folder(songToPlay);
  //     }
  //     old_encoder_value = encoder_value;
  //   }
}
void _Error_Handler(char* file, int line) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}

const struct application Application = {
    .config = config,
    .init = init,
    .run = run,
};
