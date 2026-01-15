/*
 * STM32Comm - Arduino library for STM32-ESP8266 UART communication
 *
 * A simple, callback-based library for handling command/response
 * communication between an STM32 microcontroller and ESP8266.
 *
 * Protocol:
 *   Commands from STM32: COMMAND:params\n or COMMAND\n
 *   Responses to STM32:  RESPONSE:data\n or OK\n or ERROR:message\n
 *
 * Usage:
 *   #include <STM32Comm.h>
 *
 *   STM32Comm comm;
 *
 *   void handleWifi(const char* params) {
 *     // Parse SSID,PASSWORD from params
 *     comm.sendOK();
 *   }
 *
 *   void setup() {
 *     Serial.begin(115200);
 *     comm.begin(Serial);
 *     comm.setDebugStream(Serial1);  // Optional debug output
 *     comm.onCommand("WIFI", handleWifi);
 *   }
 *
 *   void loop() {
 *     comm.process();
 *   }
 *
 * Repository: https://github.com/youruser/STM32Comm
 * License: MIT
 */

#ifndef STM32COMM_H
#define STM32COMM_H

#include <Arduino.h>

// Configuration
#ifndef STM32COMM_MAX_COMMANDS
#define STM32COMM_MAX_COMMANDS 16
#endif

#ifndef STM32COMM_MAX_CMD_LEN
#define STM32COMM_MAX_CMD_LEN 64
#endif

#ifndef STM32COMM_BUFFER_SIZE
#define STM32COMM_BUFFER_SIZE 2560
#endif

// Callback type for command handlers
// params contains everything after "COMMAND:" (or empty string if no colon)
typedef void (*STM32CommCallback)(const char* params);

class STM32Comm {
public:
    STM32Comm();

    /**
     * Initialize the communication library
     * @param serial The serial stream to use for communication (usually Serial)
     * @param rxBufferSize Optional: set RX buffer size before begin (ESP8266 only)
     */
    void begin(Stream& serial, size_t rxBufferSize = 0);

    /**
     * Set optional debug output stream
     * @param debug Stream for debug messages (usually Serial1)
     */
    void setDebugStream(Stream& debug);

    /**
     * Process incoming serial data - call this in loop()
     */
    void process();

    /**
     * Register a command handler
     * @param command The command prefix (e.g., "WIFI", "STATUS", "TIME")
     * @param callback Function to call when command is received
     * @return true if registered successfully, false if max commands reached
     */
    bool onCommand(const char* command, STM32CommCallback callback);

    /**
     * Send "OK" response
     */
    void sendOK();

    /**
     * Send error response
     * @param message Error message (sent as "ERROR:message")
     */
    void sendError(const char* message);

    /**
     * Send a raw response line
     * @param response The response string (newline added automatically)
     */
    void send(const char* response);

    /**
     * Send a formatted response
     * @param format Printf-style format string
     */
    void sendf(const char* format, ...);

    /**
     * Log a debug message (only if debug stream is set)
     * @param message Debug message
     */
    void debug(const char* message);

    /**
     * Log a formatted debug message
     * @param format Printf-style format string
     */
    void debugf(const char* format, ...);

    /**
     * Check if a command handler is registered
     * @param command The command to check
     * @return true if handler exists
     */
    bool hasCommand(const char* command);

    /**
     * Set callback for unknown commands
     * @param callback Function called with full command string if no handler found
     */
    void onUnknownCommand(STM32CommCallback callback);

private:
    Stream* _serial;
    Stream* _debug;

    // Command buffer
    char _buffer[STM32COMM_BUFFER_SIZE];
    size_t _bufferIndex;

    // Registered commands
    struct CommandEntry {
        char command[STM32COMM_MAX_CMD_LEN];
        STM32CommCallback callback;
    };
    CommandEntry _commands[STM32COMM_MAX_COMMANDS];
    uint8_t _commandCount;

    // Unknown command handler
    STM32CommCallback _unknownCallback;

    // Internal methods
    void processCommand(const char* cmd);
    void debugLogRx(const char* cmd);
    void debugLogTx(const char* response);
};

// Helper function to parse comma-separated parameters
// Returns the position after the comma, or -1 if no comma found
int stm32comm_parseParam(const char* params, char* out, size_t outSize, int startPos = 0);

// Helper to convert escaped \n to real newlines (for PEM keys, etc.)
void stm32comm_unescapeNewlines(const char* src, char* dst, size_t dstSize);

#endif // STM32COMM_H
