// // Example usage of ESP communication module with struct interface
// // This file shows how to integrate ESP communication into your application
// // following the same pattern as Application

// #include "esp_comm.h"
// #include "main.h"  // For huart2

// // Example 1: Basic usage (like Application.init() and Application.run())
// void example_basic_usage(void) {
//   // Initialize (call once in your init function)
//   EspComm.init(&huart2);

//   // In your main loop, call process regularly
//   while (1) {
//     EspComm.process();

//     // Your other code...
//   }
// }

// // Example 2: Callback-based (async) approach
// static void on_time_received(esp_time_t* time) {
//   if (time->valid) {
//     // Update your clock display
//     // printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n",
//     //        time->year, time->month, time->day,
//     //        time->hour, time->minute, time->second);
//   }
// }

// static void on_weather_received(esp_weather_t* weather) {
//   if (weather->valid) {
//     // Update weather display
//     // printf("Weather: %d°F, %s, %d%% humidity\n",
//     //        weather->temp_f, weather->condition, weather->humidity);
//   }
// }

// static void on_error(const char* error) {
//   // Handle error
//   // printf("ESP Error: %s\n", error);
// }

// void example_with_callbacks(void) {
//   // Initialize
//   EspComm.init(&huart2);

//   // Register callbacks
//   EspComm.set_time_callback(on_time_received);
//   EspComm.set_weather_callback(on_weather_received);
//   EspComm.set_error_callback(on_error);

//   // Request data
//   if (EspComm.is_ready()) {
//     EspComm.request_time();
//   }

//   // In main loop
//   while (1) {
//     EspComm.process();  // Callbacks will be called when data arrives
//   }
// }

// // Example 3: Polling mode (no callbacks)
// void example_polling_mode(void) {
//   // Initialize
//   EspComm.init(&huart2);

//   // Request time
//   if (EspComm.is_ready()) {
//     EspComm.request_time();
//   }

//   // In main loop, poll for results
//   while (1) {
//     EspComm.process();

//     const esp_time_t* time = EspComm.get_last_time();
//     if (time->valid) {
//       // Use the time data
//       // display_time(time->hour, time->minute, time->second);
//     }

//     // HAL_Delay(1000);
//   }
// }

// // Example 4: State machine (recommended for real applications)
// typedef enum {
//   ESP_STATE_IDLE,
//   ESP_STATE_WAITING_TIME,
//   ESP_STATE_WAITING_WEATHER,
//   ESP_STATE_WAITING_STOCK,
// } esp_app_state_t;

// static esp_app_state_t esp_state = ESP_STATE_IDLE;
// static uint32_t esp_last_request = 0;

// void example_state_machine_init(void) {
//   EspComm.init(&huart2);
// }

// void example_state_machine_loop(void) {
//   // Always process incoming data
//   EspComm.process();

//   uint32_t now = HAL_GetTick();

//   switch (esp_state) {
//     case ESP_STATE_IDLE:
//       // Request time every 60 seconds
//       if (now - esp_last_request > 60000) {
//         if (EspComm.is_ready() && EspComm.request_time()) {
//           esp_state = ESP_STATE_WAITING_TIME;
//           esp_last_request = now;
//         }
//       }
//       break;

//     case ESP_STATE_WAITING_TIME: {
//       const esp_time_t* time = EspComm.get_last_time();
//       if (time->valid) {
//         // Time received, update display
//         // update_clock_display(time);

//         // Now request weather
//         if (EspComm.request_weather()) {
//           esp_state = ESP_STATE_WAITING_WEATHER;
//           esp_last_request = now;
//         } else {
//           esp_state = ESP_STATE_IDLE;
//         }
//       }

//       // Timeout after 5 seconds
//       if (now - esp_last_request > 5000) {
//         esp_state = ESP_STATE_IDLE;
//       }
//       break;
//     }

//     case ESP_STATE_WAITING_WEATHER: {
//       const esp_weather_t* weather = EspComm.get_last_weather();
//       if (weather->valid) {
//         // Weather received, update display
//         // update_weather_display(weather);
//         esp_state = ESP_STATE_IDLE;
//       }

//       // Timeout after 10 seconds (HTTP can take longer)
//       if (now - esp_last_request > 10000) {
//         esp_state = ESP_STATE_IDLE;
//       }
//       break;
//     }

//     case ESP_STATE_WAITING_STOCK: {
//       const esp_stock_t* stock = EspComm.get_last_stock();
//       if (stock->valid) {
//         // Stock data received
//         // update_stock_display(stock);
//         esp_state = ESP_STATE_IDLE;
//       }

//       if (now - esp_last_request > 10000) {
//         esp_state = ESP_STATE_IDLE;
//       }
//       break;
//     }
//   }
// }

// // Example 5: Integration with your existing application.c pattern
// /*
// // In your main.c:
// #include "application.h"
// #include "esp_comm.h"

// int main(void) {
//   HAL_Init();
//   SystemClock_Config();
//   MX_GPIO_Init();
//   MX_USART2_UART_Init();  // For ESP communication

//   // Initialize application
//   Application.init();

//   // Initialize ESP communication
//   EspComm.init(&huart2);
//   EspComm.set_time_callback(on_time_received);

//   while (1) {
//     // Run application
//     Application.run();

//     // Process ESP communication
//     EspComm.process();

//     // Request updates periodically
//     static uint32_t last_update = 0;
//     if (HAL_GetTick() - last_update > 900000) {  // Every 15 minutes
//       if (EspComm.is_ready()) {
//         EspComm.request_time();
//         last_update = HAL_GetTick();
//       }
//     }
//   }
// }
// */

// // Example 6: Request multiple data types in sequence
// void example_request_all_data(void) {
//   static uint32_t last_request = 0;
//   static uint8_t request_step = 0;

//   uint32_t now = HAL_GetTick();

//   // Process responses
//   EspComm.process();

//   // Update every 5 minutes
//   if (now - last_request < 300000 && request_step == 0) {
//     return;  // Not time yet
//   }

//   switch (request_step) {
//     case 0:
//       // Request time
//       if (EspComm.is_ready() && EspComm.request_time()) {
//         request_step = 1;
//         last_request = now;
//       }
//       break;

//     case 1:
//       // Wait for time, then request weather
//       if (EspComm.get_last_time()->valid) {
//         if (EspComm.request_weather()) {
//           request_step = 2;
//         }
//       } else if (now - last_request > 5000) {
//         // Timeout, move on anyway
//         request_step = 2;
//       }
//       break;

//     case 2:
//       // Wait for weather, then request stock
//       if (EspComm.get_last_weather()->valid) {
//         if (EspComm.request_stock("AAPL")) {
//           request_step = 3;
//         }
//       } else if (now - last_request > 15000) {
//         // Timeout
//         request_step = 3;
//       }
//       break;

//     case 3:
//       // Wait for stock, then done
//       if (EspComm.get_last_stock()->valid ||
//           now - last_request > 20000) {
//         request_step = 0;  // Start over
//         last_request = now;
//       }
//       break;
//   }
// }
