/*
 * Simple ESP8266 Test Sketch (No WiFi/API Required)
 *
 * Use this to test the UART connection between ESP8266 and STM32
 * without needing WiFi or API keys.
 *
 * Upload this first to verify hardware connections work.
 */

String commandBuffer = "";

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(100);

  // Signal ready
  Serial.println("ERROR:READY");
}

void loop() {
  // Check for commands from STM32
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {
      processCommand(commandBuffer);
      commandBuffer = "";
    } else if (c != '\r') {
      commandBuffer += c;
    }
  }

  yield();
}

void processCommand(String cmd) {
  cmd.trim();

  if (cmd == "TIME") {
    // Send fake time data
    Serial.println("TIME:2026-01-08T12:34:56Z");
  }
  else if (cmd == "WEATHER") {
    // Send fake weather data
    // Format: temp_f, temp_c, condition, humidity
    Serial.println("WEATHER:72,-22,Sunny,45");
  }
  else if (cmd.startsWith("STOCK:")) {
    String symbol = cmd.substring(6);
    symbol.trim();
    // Send fake stock data
    Serial.print("STOCK:");
    Serial.print(symbol);
    Serial.println(":185.23");
  }
  else if (cmd.length() > 0) {
    Serial.println("ERROR:UNKNOWN_COMMAND");
  }
}
