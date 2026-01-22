/*
 * ESP8266 Firmware for STM32 Communication
 *
 * This sketch runs on ESP8266 and provides NTP time, weather, and stock data
 * to an STM32 microcontroller via UART (115200 baud).
 *
 * Uses the STM32Comm library for serial communication protocol handling.
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP_Google_Sheet_Client.h>
#include <STM32Comm.h>
#include <ICalParser.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

const char* DEFAULT_WIFI_SSID = "YourWiFiSSID";
const char* DEFAULT_WIFI_PASSWORD = "YourWiFiPassword";

// Weather API (OpenWeatherMap) - defaults, can be overridden via serial
#define MAX_WEATHER_API_KEY_LEN 64
#define MAX_WEATHER_CITY_LEN 64
#define MAX_WEATHER_COUNTRY_LEN 8
char weatherApiKey[MAX_WEATHER_API_KEY_LEN + 1] = "your_api_key_here";
char weatherCity[MAX_WEATHER_CITY_LEN + 1] = "San Francisco";
char weatherCountry[MAX_WEATHER_COUNTRY_LEN + 1] = "US";

// Stock API (Alpha Vantage)
const char* STOCK_API_KEY = "your_api_key_here";

// NTP Configuration
const long UTC_OFFSET_SEC = 0;  // Return UTC, STM32 handles timezone conversion
const int NTP_UPDATE_INTERVAL = 60000;

// ============================================================================
// CREDENTIALS STORAGE
// ============================================================================

#define EEPROM_SIZE 4096
#define EEPROM_MAGIC 0xAA55
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_PROJECT_ID_LEN 128
#define MAX_EMAIL_LEN 256
#define MAX_PRIVATE_KEY_LEN 2048
#define MAX_CALENDAR_URL_LEN 256

struct WiFiCredentials {
  uint16_t magic;
  char ssid[MAX_SSID_LEN + 1];
  char password[MAX_PASS_LEN + 1];
  uint8_t checksum;
};

struct GCPCredentials {
  char project_id[MAX_PROJECT_ID_LEN + 1];
  char client_email[MAX_EMAIL_LEN + 1];
  char private_key[MAX_PRIVATE_KEY_LEN + 1];
};

WiFiCredentials wifiCreds;
GCPCredentials gcpCreds;
char calendarUrl[MAX_CALENDAR_URL_LEN + 1];

// ============================================================================
// GLOBALS
// ============================================================================

STM32Comm comm;
FirebaseJson gSheetResponse;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SEC, NTP_UPDATE_INTERVAL);

// State flags
bool gsheetInitialized = false;
bool taskComplete = false;
bool gsheetInitAttempted = false;

// WiFi state machine
enum AppWiFiState { WIFI_IDLE, WIFI_CONNECTING, WIFI_CONNECTED };
AppWiFiState wifiState = WIFI_IDLE;
unsigned long wifiConnectStartTime = 0;
const unsigned long WIFI_CONNECT_TIMEOUT = 10000;
const unsigned long WIFI_CHECK_INTERVAL = 100;
unsigned long lastWifiCheck = 0;

// Cache for API responses
struct {
  unsigned long lastWeatherUpdate = 0;
  String weatherData = "";
  unsigned long lastStockUpdate = 0;
  String stockSymbol = "";
  String stockData = "";
} cache;

const unsigned long WEATHER_CACHE_TIME = 900000;  // 15 minutes
const unsigned long STOCK_CACHE_TIME = 60000;     // 1 minute

// ============================================================================
// WIFI CREDENTIALS (EEPROM)
// ============================================================================

uint8_t calculateChecksum(const WiFiCredentials& creds) {
  uint8_t sum = 0;
  const uint8_t* data = (const uint8_t*)&creds;
  for (size_t i = 0; i < sizeof(WiFiCredentials) - 1; i++) {
    sum += data[i];
  }
  return sum;
}

bool loadWiFiCredentials() {
  EEPROM.get(0, wifiCreds);
  if (wifiCreds.magic != EEPROM_MAGIC) return false;
  if (calculateChecksum(wifiCreds) != wifiCreds.checksum) return false;
  return true;
}

void saveWiFiCredentials(const char* ssid, const char* password) {
  wifiCreds.magic = EEPROM_MAGIC;
  strncpy(wifiCreds.ssid, ssid, MAX_SSID_LEN);
  wifiCreds.ssid[MAX_SSID_LEN] = '\0';
  strncpy(wifiCreds.password, password, MAX_PASS_LEN);
  wifiCreds.password[MAX_PASS_LEN] = '\0';
  wifiCreds.checksum = calculateChecksum(wifiCreds);
  EEPROM.put(0, wifiCreds);
  EEPROM.commit();
}

// ============================================================================
// GCP CREDENTIALS (RAM only)
// ============================================================================

void tryInitGSheet() {
  if (gsheetInitialized) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (!gsheetInitAttempted) {
    gsheetInitAttempted = true;
    comm.debug("WiFi connected, checking GCP credentials...");
    comm.debugf("project_id len: %d", strlen(gcpCreds.project_id));
    comm.debugf("client_email len: %d", strlen(gcpCreds.client_email));
    comm.debugf("private_key len: %d", strlen(gcpCreds.private_key));
  }

  if (strlen(gcpCreds.project_id) == 0 ||
      strlen(gcpCreds.client_email) == 0 ||
      strlen(gcpCreds.private_key) == 0) {
    return;
  }

  comm.debug("Initializing GSheet with credentials");
  GSheet.begin(gcpCreds.client_email, gcpCreds.project_id, gcpCreds.private_key);
  gsheetInitialized = true;
  comm.debug("GSheet initialized, waiting for auth...");
}


// ============================================================================
// WIFI CONNECTION (NON-BLOCKING)
// ============================================================================

void startWiFiConnect() {
  if (wifiState == WIFI_CONNECTING) return;

  comm.debugf("Starting WiFi connection to: %s", wifiCreds.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiCreds.ssid, wifiCreds.password);

  wifiState = WIFI_CONNECTING;
  wifiConnectStartTime = millis();
  lastWifiCheck = millis();
}

void handleAppWiFiState() {
  unsigned long now = millis();

  switch (wifiState) {
    case WIFI_IDLE:
      if (WiFi.status() != WL_CONNECTED) {
        startWiFiConnect();
      }
      break;

    case WIFI_CONNECTING:
      if (now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
        lastWifiCheck = now;
        if (WiFi.status() == WL_CONNECTED) {
          wifiState = WIFI_CONNECTED;
          comm.debugf("WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
        } else if (now - wifiConnectStartTime >= WIFI_CONNECT_TIMEOUT) {
          wifiState = WIFI_IDLE;
          comm.debug("WiFi connection timeout");
          comm.sendError("WIFI_CONNECT_FAILED");
        }
      }
      break;

    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        comm.debug("WiFi connection lost");
        wifiState = WIFI_IDLE;
      }
      break;
  }
}

// ============================================================================
// GOOGLE SHEETS
// ============================================================================

int getBalance() {
  gSheetResponse.clear();
  if (GSheet.values.get(&gSheetResponse,
                        "17LL2-dGZ4IDsxmZm_Vnl6F9BJ2zrWcfUP-DBS8n2ZLY",
                        "Sheet1!F2:F2")) {
    FirebaseJsonData jsonData;
    if (gSheetResponse.get(jsonData, "values/[0]/[0]")) {
      return jsonData.to<int>();
    }
  }
  return -1;
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void handleWifiCommand(const char* params) {
  // Parse SSID,PASSWORD
  char ssid[MAX_SSID_LEN + 1];
  char password[MAX_PASS_LEN + 1];

  int nextPos = stm32comm_parseParam(params, ssid, sizeof(ssid), 0);
  if (nextPos < 0) {
    comm.sendError("INVALID_WIFI_FORMAT");
    return;
  }
  stm32comm_parseParam(params, password, sizeof(password), nextPos);

  if (strlen(ssid) == 0 || strlen(ssid) > MAX_SSID_LEN || strlen(password) > MAX_PASS_LEN) {
    comm.sendError("INVALID_WIFI_PARAMS");
    return;
  }

  // Save and reconnect
  saveWiFiCredentials(ssid, password);
  strncpy(wifiCreds.ssid, ssid, MAX_SSID_LEN);
  strncpy(wifiCreds.password, password, MAX_PASS_LEN);

  WiFi.disconnect();
  wifiState = WIFI_IDLE;
  startWiFiConnect();

  comm.sendOK();
}

void handleStatusCommand(const char* params) {
  (void)params;

  // Determine GSheet status
  const char* gsheetStatus;
  if (!gsheetInitialized) {
    gsheetStatus = "GSHEET_NOT_INIT";
  } else if (GSheet.ready()) {
    gsheetStatus = "GSHEET_READY";
  } else {
    gsheetStatus = "GSHEET_AUTH_PENDING";
  }

  if (WiFi.status() == WL_CONNECTED) {
    comm.sendf("STATUS:CONNECTED,%s,%d,%s", WiFi.localIP().toString().c_str(), WiFi.RSSI(), gsheetStatus);
  } else if (wifiState == WIFI_CONNECTING) {
    comm.sendf("STATUS:CONNECTING,%s", gsheetStatus);
  } else {
    comm.sendf("STATUS:DISCONNECTED,%s", gsheetStatus);
  }
}

void handleTimeCommand(const char* params) {
  (void)params;
  if (!timeClient.isTimeSet()) {
    timeClient.forceUpdate();
  }

  if (!timeClient.isTimeSet()) {
    comm.sendError("NTP_FAILED");
    return;
  }

  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = epochTime;
  struct tm* timeInfo = gmtime(&rawTime);

  comm.sendf("TIME:%04d-%02d-%02dT%02d:%02d:%02dZ",
             timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday,
             timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
}

void handleWeatherCommand(const char* params) {
  (void)params;

  // Check cache
  unsigned long now = millis();
  if (cache.weatherData.length() > 0 &&
      (now - cache.lastWeatherUpdate) < WEATHER_CACHE_TIME) {
    comm.send(cache.weatherData.c_str());
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    comm.sendError("NO_WIFI");
    return;
  }

  // Check if weather API key is configured
  if (strlen(weatherApiKey) == 0 || strcmp(weatherApiKey, "your_api_key_here") == 0) {
    comm.sendError("WEATHER_API_KEY_NOT_SET");
    return;
  }

  // Use forecast API to get precipitation probability (pop)
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=";
  url += weatherCity;
  url += ",";
  url += weatherCountry;
  url += "&appid=";
  url += weatherApiKey;
  url += "&units=metric&cnt=1";  // Only get first forecast period

  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode != 200) {
    http.end();
    comm.sendf("ERROR:HTTP_%d", httpCode);
    return;
  }

  String payload = http.getString();
  http.end();

  // Forecast API response is larger than current weather API
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    comm.debugf("JSON parse error: %s", err.c_str());
    comm.sendError("JSON_PARSE");
    return;
  }

  // Forecast API returns data in list[0] for first period
  JsonObject forecast = doc["list"][0];
  float temp_c = forecast["main"]["temp"];
  int temp_f = (int)(temp_c * 9.0 / 5.0 + 32.0);
  const char* condition = forecast["weather"][0]["main"];
  int humidity = forecast["main"]["humidity"];

  // pop is probability of precipitation (0.0 to 1.0)
  float pop = forecast["pop"] | 0.0;
  int precip_chance = (int)(pop * 100);

  String response = "WEATHER:";
  response += String(temp_f) + "," + String((int)temp_c) + "," + String(condition) + "," + String(humidity) + "," + String(precip_chance);

  cache.weatherData = response;
  cache.lastWeatherUpdate = now;

  comm.send(response.c_str());
}

void handleStockCommand(const char* params) {
  String symbol = String(params);
  symbol.trim();
  symbol.toUpperCase();

  unsigned long now = millis();
  if (symbol == cache.stockSymbol && cache.stockData.length() > 0 &&
      (now - cache.lastStockUpdate) < STOCK_CACHE_TIME) {
    comm.send(cache.stockData.c_str());
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    comm.sendError("NO_WIFI");
    return;
  }

  String url = "http://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=";
  url += symbol;
  url += "&apikey=";
  url += STOCK_API_KEY;

  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode != 200) {
    http.end();
    comm.sendf("ERROR:HTTP_%d", httpCode);
    return;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, payload)) {
    comm.sendError("JSON_PARSE");
    return;
  }

  const char* priceStr = doc["Global Quote"]["05. price"];
  if (!priceStr) {
    comm.sendError("INVALID_SYMBOL");
    return;
  }

  float price = atof(priceStr);
  String response = "STOCK:" + symbol + ":" + String(price, 2);

  cache.stockSymbol = symbol;
  cache.stockData = response;
  cache.lastStockUpdate = now;

  comm.send(response.c_str());
}

void handleBalanceCommand(const char* params) {
  (void)params;

  if (!gsheetInitialized) {
    comm.sendError("GSHEET_NOT_INIT");
    return;
  }

  if (!GSheet.ready()) {
    comm.sendError("GSHEET_NOT_READY");
    return;
  }

  int balance = getBalance();
  if (balance >= 0) {
    comm.sendf("BALANCE:%d", balance);
  } else {
    comm.sendError("BALANCE_QUERY_FAILED");
  }
}

void handleGCPProjectCommand(const char* params) {
  if (strlen(params) == 0 || strlen(params) > MAX_PROJECT_ID_LEN) {
    comm.sendError("INVALID_PROJECT_ID");
    return;
  }
  strncpy(gcpCreds.project_id, params, MAX_PROJECT_ID_LEN);
  gcpCreds.project_id[MAX_PROJECT_ID_LEN] = '\0';
  tryInitGSheet();
  comm.sendOK();
}

void handleGCPEmailCommand(const char* params) {
  if (strlen(params) == 0 || strlen(params) > MAX_EMAIL_LEN) {
    comm.sendError("INVALID_EMAIL");
    return;
  }
  strncpy(gcpCreds.client_email, params, MAX_EMAIL_LEN);
  gcpCreds.client_email[MAX_EMAIL_LEN] = '\0';
  tryInitGSheet();
  comm.sendOK();
}

void handleGCPKeyCommand(const char* params) {
  if (strlen(params) == 0 || strlen(params) > MAX_PRIVATE_KEY_LEN) {
    comm.sendError("INVALID_KEY");
    return;
  }
  // Convert \n literals to actual newlines for PEM format
  stm32comm_unescapeNewlines(params, gcpCreds.private_key, MAX_PRIVATE_KEY_LEN + 1);
  comm.debugf("Private key converted, new len: %d", strlen(gcpCreds.private_key));
  tryInitGSheet();
  comm.sendOK();
}

void handleSetCalendarUrlCommand(const char* params) {
  if (strlen(params) == 0 || strlen(params) > MAX_CALENDAR_URL_LEN) {
    comm.sendError("INVALID_CALENDAR_URL");
    return;
  }
  strncpy(calendarUrl, params, MAX_CALENDAR_URL_LEN);
  calendarUrl[MAX_CALENDAR_URL_LEN] = '\0';
  comm.debugf("Calendar URL set, len: %d", strlen(calendarUrl));
  comm.sendOK();
}

void handleSetWeatherApiKeyCommand(const char* params) {
  if (strlen(params) == 0 || strlen(params) > MAX_WEATHER_API_KEY_LEN) {
    comm.sendError("INVALID_WEATHER_API_KEY");
    return;
  }
  strncpy(weatherApiKey, params, MAX_WEATHER_API_KEY_LEN);
  weatherApiKey[MAX_WEATHER_API_KEY_LEN] = '\0';
  // Clear weather cache when API key changes
  cache.weatherData = "";
  cache.lastWeatherUpdate = 0;
  comm.debugf("Weather API key set, len: %d", strlen(weatherApiKey));
  comm.sendOK();
}

void handleSetWeatherLocationCommand(const char* params) {
  // Parse city,country
  char city[MAX_WEATHER_CITY_LEN + 1];
  char country[MAX_WEATHER_COUNTRY_LEN + 1];

  int nextPos = stm32comm_parseParam(params, city, sizeof(city), 0);
  if (nextPos < 0) {
    comm.sendError("INVALID_WEATHER_LOCATION_FORMAT");
    return;
  }
  stm32comm_parseParam(params, country, sizeof(country), nextPos);

  if (strlen(city) == 0 || strlen(city) > MAX_WEATHER_CITY_LEN ||
      strlen(country) == 0 || strlen(country) > MAX_WEATHER_COUNTRY_LEN) {
    comm.sendError("INVALID_WEATHER_LOCATION_PARAMS");
    return;
  }

  strncpy(weatherCity, city, MAX_WEATHER_CITY_LEN);
  weatherCity[MAX_WEATHER_CITY_LEN] = '\0';
  strncpy(weatherCountry, country, MAX_WEATHER_COUNTRY_LEN);
  weatherCountry[MAX_WEATHER_COUNTRY_LEN] = '\0';

  // Clear weather cache when location changes
  cache.weatherData = "";
  cache.lastWeatherUpdate = 0;

  comm.debugf("Weather location set: %s, %s", weatherCity, weatherCountry);
  comm.sendOK();
}

void handleCalendarCommand(const char* params) {
  // Parse optional event count parameter (default 10)
  int maxEvents = 10;
  if (params && strlen(params) > 0) {
    maxEvents = atoi(params);
    if (maxEvents < 1) maxEvents = 1;
    if (maxEvents > 20) maxEvents = 20;
  }

  if (strlen(calendarUrl) == 0) {
    comm.sendError("CALENDAR_URL_NOT_SET");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    comm.sendError("NO_WIFI");
    return;
  }

  comm.debug("Fetching calendar...");
  comm.debugf("Free heap: %d", ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(4096, 512);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.setTimeout(15000);

  if (!http.begin(client, calendarUrl)) {
    comm.sendError("HTTP_BEGIN_FAILED");
    return;
  }

  http.addHeader("User-Agent", "ESP8266");
  http.addHeader("Accept", "*/*");

  comm.debug("Sending request...");
  int httpCode = http.GET();
  comm.debugf("HTTP code: %d", httpCode);

  if (httpCode != 200) {
    http.end();
    comm.sendf("ERROR:HTTP_%d", httpCode);
    return;
  }

  // Stream the response to avoid memory issues
  WiFiClient* stream = http.getStreamPtr();
  time_t now = timeClient.getEpochTime();

  // Use static storage to reduce stack usage (reduced from 20 to save memory)
  static ICalEvent calEvents[12];
  int calEventCount = 0;
  if (maxEvents > 12) maxEvents = 12;

  // Use static buffers instead of String to reduce heap fragmentation
  static char lineBuf[257];
  static char currentDtStart[32];
  static char currentDtEnd[32];
  static char currentSummary[ICAL_MAX_TITLE_LEN];
  static char currentRRule[128];
  int lineLen = 0;
  currentDtStart[0] = '\0';
  currentDtEnd[0] = '\0';
  currentSummary[0] = '\0';
  currentRRule[0] = '\0';
  bool inEvent = false;
  bool currentCancelled = false;
  bool hasRecurrenceId = false;

  comm.debugf("Parsing (max %d events)...", maxEvents);
  yield();  // Prevent watchdog reset
  comm.debugf("Heap before parse: %d", ESP.getFreeHeap());

  unsigned long parseStart = millis();
  unsigned long lastData = millis();
  const unsigned long PARSE_TIMEOUT = 30000;
  const unsigned long DATA_TIMEOUT = 5000;
  int lineCount = 0;
  int eventsSeen = 0;
  int recurringCount = 0;

  while (millis() - parseStart < PARSE_TIMEOUT) {
    if (stream->available()) {
      lastData = millis();
      char c = stream->read();

      if (c == '\n') {
        lineBuf[lineLen] = '\0';
        // Trim trailing whitespace
        while (lineLen > 0 && (lineBuf[lineLen-1] == ' ' || lineBuf[lineLen-1] == '\t')) {
          lineBuf[--lineLen] = '\0';
        }
        lineCount++;

        if (strcmp(lineBuf, "BEGIN:VEVENT") == 0) {
          inEvent = true;
          currentDtStart[0] = '\0';
          currentDtEnd[0] = '\0';
          currentSummary[0] = '\0';
          currentRRule[0] = '\0';
          currentCancelled = false;
          hasRecurrenceId = false;
        } else if (strcmp(lineBuf, "END:VEVENT") == 0 && inEvent) {
          inEvent = false;
          eventsSeen++;

          if (currentCancelled || hasRecurrenceId) {
            lineLen = 0;
            continue;
          }

          if (strlen(currentDtStart) > 0 && strlen(currentSummary) > 0) {
            time_t dtstart = ICalParser::parseDate(currentDtStart);
            time_t dtend = (strlen(currentDtEnd) > 0) ? ICalParser::parseDate(currentDtEnd) : dtstart;
            time_t duration = dtend - dtstart;  // Duration in seconds

            ICalRRule rule = ICalParser::parseRRule(currentRRule);

            if (rule.freq != ICAL_FREQ_NONE) {
              recurringCount++;
            }

            time_t nextOccur = ICalParser::getNextOccurrence(dtstart, rule, now);

            if (nextOccur > 0) {
              time_t endOccur = nextOccur + duration;  // Calculate end time

              // Insert sorted
              int insertIdx = calEventCount;
              for (int i = 0; i < calEventCount; i++) {
                if (nextOccur < calEvents[i].occurrence) {
                  insertIdx = i;
                  break;
                }
              }

              if (insertIdx < maxEvents) {
                int shiftEnd = (calEventCount < maxEvents) ? calEventCount : maxEvents - 1;
                for (int i = shiftEnd; i > insertIdx; i--) {
                  calEvents[i] = calEvents[i - 1];
                }

                calEvents[insertIdx].occurrence = nextOccur;
                calEvents[insertIdx].endOccurrence = endOccur;

                struct tm* tmInfo = localtime(&nextOccur);
                snprintf(calEvents[insertIdx].datetime, 20, "%04d-%02d-%02d %02d:%02d",
                         tmInfo->tm_year + 1900, tmInfo->tm_mon + 1, tmInfo->tm_mday,
                         tmInfo->tm_hour, tmInfo->tm_min);

                struct tm* endTmInfo = localtime(&endOccur);
                snprintf(calEvents[insertIdx].endDatetime, 20, "%04d-%02d-%02d %02d:%02d",
                         endTmInfo->tm_year + 1900, endTmInfo->tm_mon + 1, endTmInfo->tm_mday,
                         endTmInfo->tm_hour, endTmInfo->tm_min);

                strncpy(calEvents[insertIdx].title, currentSummary, ICAL_MAX_TITLE_LEN - 1);
                calEvents[insertIdx].title[ICAL_MAX_TITLE_LEN - 1] = '\0';

                if (calEventCount < maxEvents) calEventCount++;
              }
            }
          }
        } else if (inEvent) {
          if (strncmp(lineBuf, "DTSTART", 7) == 0) {
            char* colon = strchr(lineBuf, ':');
            if (colon) {
              strncpy(currentDtStart, colon + 1, sizeof(currentDtStart) - 1);
              currentDtStart[sizeof(currentDtStart) - 1] = '\0';
            }
          } else if (strncmp(lineBuf, "DTEND", 5) == 0) {
            char* colon = strchr(lineBuf, ':');
            if (colon) {
              strncpy(currentDtEnd, colon + 1, sizeof(currentDtEnd) - 1);
              currentDtEnd[sizeof(currentDtEnd) - 1] = '\0';
            }
          } else if (strncmp(lineBuf, "SUMMARY:", 8) == 0) {
            strncpy(currentSummary, lineBuf + 8, sizeof(currentSummary) - 1);
            currentSummary[sizeof(currentSummary) - 1] = '\0';
          } else if (strncmp(lineBuf, "RRULE:", 6) == 0) {
            strncpy(currentRRule, lineBuf + 6, sizeof(currentRRule) - 1);
            currentRRule[sizeof(currentRRule) - 1] = '\0';
          } else if (strncmp(lineBuf, "STATUS:", 7) == 0) {
            if (strstr(lineBuf, "CANCELLED") != nullptr) {
              currentCancelled = true;
            }
          } else if (strncmp(lineBuf, "RECURRENCE-ID", 13) == 0) {
            hasRecurrenceId = true;
          }
        }

        lineLen = 0;
        yield();
      } else if (c != '\r') {
        if (lineLen < 256) {
          lineBuf[lineLen++] = c;
        }
      }
    } else {
      if (millis() - lastData > DATA_TIMEOUT) {
        comm.debug("Data timeout");
        break;
      }
      if (!http.connected()) {
        break;
      }
      yield();
      delay(1);
    }
  }

  comm.debugf("Parsed %d lines, %d events (%d recurring)", lineCount, eventsSeen, recurringCount);
  http.end();
  comm.debugf("Found %d upcoming events", calEventCount);

  // Build response: CALENDAR:count,start|end|title;start|end|title;...
  if (calEventCount == 0) {
    comm.send("CALENDAR:0");
  } else {
    String response = "CALENDAR:";
    response += String(calEventCount);
    for (int i = 0; i < calEventCount; i++) {
      response += (i == 0) ? "," : ";";
      response += calEvents[i].datetime;
      response += "|";
      response += calEvents[i].endDatetime;
      response += "|";
      response += calEvents[i].title;
    }
    comm.send(response.c_str());
  }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Set large RX buffer before Serial.begin()
  Serial.setRxBufferSize(2048);
  Serial.begin(115200);
  Serial1.begin(115200);
  Serial.setTimeout(100);

  delay(100);

  // Initialize communication library
  comm.begin(Serial);
  comm.setDebugStream(Serial1);

  // Register command handlers
  comm.onCommand("WIFI", handleWifiCommand);
  comm.onCommand("STATUS", handleStatusCommand);
  comm.onCommand("TIME", handleTimeCommand);
  comm.onCommand("WEATHER", handleWeatherCommand);
  comm.onCommand("STOCK", handleStockCommand);
  comm.onCommand("BALANCE", handleBalanceCommand);
  comm.onCommand("CALENDAR", handleCalendarCommand);
  comm.onCommand("SET_CALENDAR_URL", handleSetCalendarUrlCommand);
  comm.onCommand("SET_WEATHER_API_KEY", handleSetWeatherApiKeyCommand);
  comm.onCommand("SET_WEATHER_LOCATION", handleSetWeatherLocationCommand);
  comm.onCommand("GCP_PROJECT", handleGCPProjectCommand);
  comm.onCommand("GCP_EMAIL", handleGCPEmailCommand);
  comm.onCommand("GCP_KEY", handleGCPKeyCommand);

  // Initialize EEPROM and credentials
  EEPROM.begin(EEPROM_SIZE);
  memset(&gcpCreds, 0, sizeof(gcpCreds));
  memset(calendarUrl, 0, sizeof(calendarUrl));

  if (!loadWiFiCredentials()) {
    strncpy(wifiCreds.ssid, DEFAULT_WIFI_SSID, MAX_SSID_LEN);
    strncpy(wifiCreds.password, DEFAULT_WIFI_PASSWORD, MAX_PASS_LEN);
  }

  // Start WiFi and NTP
  startWiFiConnect();
  timeClient.begin();

  comm.sendError("READY");  // Signal ready to STM32
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Handle WiFi state machine
  handleAppWiFiState();

  // Process incoming commands
  comm.process();

  // Keep NTP updated when connected
  if (wifiState == WIFI_CONNECTED) {
    timeClient.update();
  }

  // Try to initialize GSheet
  if (!gsheetInitialized) {
    tryInitGSheet();
  }

  // Fetch balance once GSheet is ready
  if (gsheetInitialized && !taskComplete) {
    static unsigned long lastAttempt = 0;
    static bool authLogged = false;
    unsigned long now = millis();

    if (GSheet.ready()) {
      if (!authLogged) {
        authLogged = true;
        comm.debug("GSheet.ready() returned true - auth complete!");
      }

      // Retry every 5 seconds if previous attempt failed
      if (now - lastAttempt >= 5000) {
        lastAttempt = now;
        int balance = getBalance();
        if (balance >= 0) {
          comm.debugf("Balance: %d", balance);
          taskComplete = true;
        } else {
          comm.debug("getBalance failed, will retry...");
        }
      }
    } else {
      if (now - lastAttempt >= 5000) {
        lastAttempt = now;
        comm.debug("Waiting for GSheet auth...");
      }
    }
  }

  yield();
}
