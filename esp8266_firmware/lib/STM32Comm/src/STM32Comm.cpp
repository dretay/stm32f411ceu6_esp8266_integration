/*
 * STM32Comm - Arduino library for STM32-ESP8266 UART communication
 * Implementation file
 */

#include "STM32Comm.h"
#include <stdarg.h>

STM32Comm::STM32Comm()
    : _serial(nullptr)
    , _debug(nullptr)
    , _bufferIndex(0)
    , _commandCount(0)
    , _unknownCallback(nullptr)
{
    memset(_buffer, 0, sizeof(_buffer));
    memset(_commands, 0, sizeof(_commands));
}

void STM32Comm::begin(Stream& serial, size_t rxBufferSize) {
    _serial = &serial;
    _bufferIndex = 0;

#ifdef ESP8266
    // For ESP8266, we can set RX buffer size if using HardwareSerial
    if (rxBufferSize > 0) {
        // Note: setRxBufferSize must be called before Serial.begin()
        // This is a hint for users - they should call it themselves
        // before begin() if they need a larger buffer
    }
#endif
    (void)rxBufferSize; // Suppress unused warning
}

void STM32Comm::setDebugStream(Stream& debug) {
    _debug = &debug;
}

void STM32Comm::process() {
    if (!_serial) return;

    while (_serial->available()) {
        char c = _serial->read();

        if (c == '\n') {
            // Command complete
            _buffer[_bufferIndex] = '\0';

            // Log received command
            debugLogRx(_buffer);

            // Process it
            processCommand(_buffer);

            // Reset buffer
            _bufferIndex = 0;
            _buffer[0] = '\0';
        } else if (c != '\r') {
            // Add to buffer (ignore carriage returns)
            if (_bufferIndex < STM32COMM_BUFFER_SIZE - 1) {
                _buffer[_bufferIndex++] = c;
            }
        }
    }
}

void STM32Comm::processCommand(const char* cmd) {
    // Skip empty commands
    if (cmd[0] == '\0') return;

    // Find the colon (if any) to separate command from params
    const char* colonPos = strchr(cmd, ':');
    char commandName[STM32COMM_MAX_CMD_LEN];
    const char* params = "";

    if (colonPos) {
        // Copy command name (before colon)
        size_t cmdLen = colonPos - cmd;
        if (cmdLen >= STM32COMM_MAX_CMD_LEN) {
            cmdLen = STM32COMM_MAX_CMD_LEN - 1;
        }
        strncpy(commandName, cmd, cmdLen);
        commandName[cmdLen] = '\0';
        params = colonPos + 1;
    } else {
        // No colon - entire string is command name
        strncpy(commandName, cmd, STM32COMM_MAX_CMD_LEN - 1);
        commandName[STM32COMM_MAX_CMD_LEN - 1] = '\0';
    }

    // Look for registered handler
    for (uint8_t i = 0; i < _commandCount; i++) {
        if (strcmp(_commands[i].command, commandName) == 0) {
            if (_commands[i].callback) {
                _commands[i].callback(params);
            }
            return;
        }
    }

    // No handler found - try unknown command callback
    if (_unknownCallback) {
        _unknownCallback(cmd);
    } else {
        sendError("UNKNOWN_COMMAND");
    }
}

bool STM32Comm::onCommand(const char* command, STM32CommCallback callback) {
    if (_commandCount >= STM32COMM_MAX_COMMANDS) {
        return false;
    }

    // Check if command already registered (update it)
    for (uint8_t i = 0; i < _commandCount; i++) {
        if (strcmp(_commands[i].command, command) == 0) {
            _commands[i].callback = callback;
            return true;
        }
    }

    // Add new command
    strncpy(_commands[_commandCount].command, command, STM32COMM_MAX_CMD_LEN - 1);
    _commands[_commandCount].command[STM32COMM_MAX_CMD_LEN - 1] = '\0';
    _commands[_commandCount].callback = callback;
    _commandCount++;

    return true;
}

void STM32Comm::onUnknownCommand(STM32CommCallback callback) {
    _unknownCallback = callback;
}

bool STM32Comm::hasCommand(const char* command) {
    for (uint8_t i = 0; i < _commandCount; i++) {
        if (strcmp(_commands[i].command, command) == 0) {
            return true;
        }
    }
    return false;
}

void STM32Comm::sendOK() {
    send("OK");
}

void STM32Comm::sendError(const char* message) {
    if (_serial) {
        _serial->print("ERROR:");
        _serial->println(message);
        debugLogTx((String("ERROR:") + message).c_str());
    }
}

void STM32Comm::send(const char* response) {
    if (_serial) {
        _serial->println(response);
        debugLogTx(response);
    }
}

void STM32Comm::sendf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    send(buffer);
}

void STM32Comm::debug(const char* message) {
    if (_debug) {
        _debug->print("DBG: ");
        _debug->println(message);
    }
}

void STM32Comm::debugf(const char* format, ...) {
    if (_debug) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        debug(buffer);
    }
}

void STM32Comm::debugLogRx(const char* cmd) {
    if (_debug) {
        _debug->print("RX> ");
        _debug->println(cmd);
    }
}

void STM32Comm::debugLogTx(const char* response) {
    if (_debug) {
        _debug->print("TX> ");
        _debug->println(response);
    }
}

// Helper function implementations

int stm32comm_parseParam(const char* params, char* out, size_t outSize, int startPos) {
    if (!params || !out || outSize == 0) return -1;

    const char* start = params + startPos;
    const char* comma = strchr(start, ',');

    size_t len;
    int nextPos;

    if (comma) {
        len = comma - start;
        nextPos = (comma - params) + 1;
    } else {
        len = strlen(start);
        nextPos = -1; // No more params
    }

    if (len >= outSize) {
        len = outSize - 1;
    }

    strncpy(out, start, len);
    out[len] = '\0';

    return nextPos;
}

void stm32comm_unescapeNewlines(const char* src, char* dst, size_t dstSize) {
    if (!src || !dst || dstSize == 0) return;

    size_t srcIdx = 0;
    size_t dstIdx = 0;
    size_t srcLen = strlen(src);

    while (srcIdx < srcLen && dstIdx < dstSize - 1) {
        if (src[srcIdx] == '\\' && srcIdx + 1 < srcLen && src[srcIdx + 1] == 'n') {
            dst[dstIdx++] = '\n';
            srcIdx += 2;
        } else {
            dst[dstIdx++] = src[srcIdx++];
        }
    }
    dst[dstIdx] = '\0';
}
