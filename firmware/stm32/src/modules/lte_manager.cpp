#include "lte_manager.h"
#include "config.h"

#define AT_TIMEOUT_MS       1000
#define RX_POLL_INTERVAL_MS 100

// AT traffic is printed only when debug serial output is enabled
#define LTE_AT_LOG (DEBUG_SERIAL && DEBUG_LTE_AT)

static Uart lteSerial(PIN_LTE_RX, PIN_LTE_TX);    // UART4

// Text received from the module by the last waitFor()
static char response[256];
static size_t responseLength = 0;

// ----------------------------------------------------------------------------
// Debug helpers
// ----------------------------------------------------------------------------

#if DEBUG_SERIAL
static void printHex(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}
#endif

#if LTE_AT_LOG
// Prints module output with control characters made visible:
// \r \n shown as text, other non-printable bytes as <XX> (garbage = wrong baud rate)
static void printEscaped(const char* prefix, const char* text, size_t length) {
    Serial.print(prefix);
    for (size_t i = 0; i < length; i++) {
        uint8_t c = (uint8_t)text[i];
        if (c == '\r') {
            Serial.print("\\r");
        } else if (c == '\n') {
            Serial.print("\\n");
        } else if (c >= 0x20 && c < 0x7F) {
            Serial.print((char)c);
        } else {
            Serial.print('<');
            if (c < 0x10) Serial.print('0');
            Serial.print(c, HEX);
            Serial.print('>');
        }
    }
    Serial.println();
}
#endif

// ----------------------------------------------------------------------------
// AT command helpers
// ----------------------------------------------------------------------------

// Discards everything waiting in the UART buffer (URCs received in the meantime)
static void clearInput() {
    #if LTE_AT_LOG
        char discarded[64];
        size_t count = 0;
    #endif
    while (lteSerial.available()) {
        char c = (char)lteSerial.read();
        #if LTE_AT_LOG
            if (count < sizeof(discarded)) discarded[count++] = c;
        #else
            (void)c;
        #endif
    }
    #if LTE_AT_LOG
        if (count > 0) printEscaped("<< (unsolicited) ", discarded, count);
    #endif
}

// Collects module output into response[] until 'token' appears.
// Stops early on "ERROR". Characters after the token stay in the UART buffer.
static bool waitFor(const char* token, uint32_t timeoutMs) {
    responseLength = 0;
    response[0] = '\0';
    bool found = false;

    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (!lteSerial.available()) {
            continue;
        }
        char c = (char)lteSerial.read();
        if (responseLength < sizeof(response) - 1) {
            response[responseLength++] = c;
            response[responseLength] = '\0';
        }
        if (strstr(response, token) != nullptr) {
            found = true;
            break;
        }
        if (strstr(response, "ERROR\r\n") != nullptr) {
            break;
        }
    }

    #if LTE_AT_LOG
        printEscaped("<< ", response, responseLength);
        if (!found && strstr(response, "ERROR") == nullptr) {
            Serial.println(responseLength == 0 ? "   (timeout - no response)" : "   (timeout)");
        }
    #endif
    return found;
}

// Sends a command line without waiting for the answer
static void sendCommand(const char* command) {
    clearInput();
    #if LTE_AT_LOG
        Serial.print(">> ");
        Serial.println(command);
    #endif
    lteSerial.print(command);
    lteSerial.print("\r");
}

// Sends an AT command and waits for its final result.
// Returns true on "OK". The whole answer is available in response[].
static bool sendAT(const char* command, uint32_t timeoutMs = AT_TIMEOUT_MS) {
    sendCommand(command);
    return waitFor("OK\r\n", timeoutMs);
}

// Reads a decimal number from the module, terminated by any non-digit character
static int readNumber(uint32_t timeoutMs) {
    int value = 0;
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (!lteSerial.available()) {
            continue;
        }
        char c = (char)lteSerial.read();
        if (c < '0' || c > '9') {
            return value;
        }
        value = value * 10 + (c - '0');
    }
    return -1;
}

// ----------------------------------------------------------------------------
// Module set-up
// ----------------------------------------------------------------------------

// True if registered in the network. +CEREG: <n>,<stat>   stat 1 = home network, 5 = roaming
static bool isRegistered() {
    return sendAT("AT+CEREG?") &&
           (strstr(response, "+CEREG: 0,1") != nullptr || strstr(response, "+CEREG: 0,5") != nullptr);
}

static bool waitForRegistration(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (isRegistered()) {
            return true;
        }
        delay(1000);
    }
    return false;
}

// Opens UDP socket 0 on PDP context 0
static bool openSocket() {
    char command[96];
    snprintf(command, sizeof(command), "AT+CAOPEN=0,0,\"UDP\",\"%s\",%d",
             LTE_SERVER_HOST, LTE_SERVER_PORT);

    sendAT("AT+CACLOSE=0");     // close a socket left from earlier (ERROR if none - fine)
    return sendAT(command, 15000) && strstr(response, "+CAOPEN: 0,0") != nullptr;
}

#if DEBUG_SERIAL
    #define LTE_FAIL(message) do { Serial.println(message); return false; } while (0)
#else
    #define LTE_FAIL(message) return false
#endif

bool lteInit() {
    lteSerial.begin(LTE_BAUDRATE);

    // Module may still be booting - wait until it answers
    bool alive = false;
    for (uint8_t i = 0; i < 20 && !alive; i++) {
        alive = sendAT("AT", 500);
    }
    if (!alive) LTE_FAIL("SIM7080G not responding.");

    sendAT("ATE0");                                         // no echo - easier parsing

    if (!sendAT("AT+CPIN?", 5000) || strstr(response, "READY") == nullptr) {
        LTE_FAIL("SIM card not ready.");
    }

    sendAT("AT+CMNB=1");                                    // CAT-M only
    sendAT("AT+CGDCONT=1,\"IP\",\"" LTE_APN "\"");          // PDP context, IPv4
    sendAT("AT+CNCFG=0,1,\"" LTE_APN "\"");                 // app network profile 0

    if (!waitForRegistration(LTE_REGISTER_TIMEOUT_MS)) {
        LTE_FAIL("LTE-M network registration failed.");
    }

    // Activate PDP context 0 (unless it is already active)
    sendAT("AT+CNACT?");
    if (strstr(response, "+CNACT: 0,1") == nullptr) {
        if (!sendAT("AT+CNACT=0,1") || !waitFor("+APP PDP: 0,ACTIVE", 30000)) {
            LTE_FAIL("PDP context activation failed.");
        }
    }

    sendAT("AT+CASRIP=0");      // received data without remote IP/port prefix

    if (!openSocket()) LTE_FAIL("UDP socket open failed.");

    #if DEBUG_SERIAL
        Serial.println("SIM7080G initialized, UDP socket open.");
    #endif
    return true;
}

bool lteGetSignal(int* rsrp, int* sinr) {
    if (!isRegistered() || !sendAT("AT+CPSI?")) {
        return false;
    }

    // +CPSI: LTE CAT-M1,Online,<MCC-MNC>,<TAC>,<SCellID>,<PCellID>,<Band>,<earfcn>,
    //        <dlbw>,<ulbw>,<RSRQ>,<RSRP>,<RSSI>,<RSSNR>
    const char* line = strstr(response, "+CPSI: LTE CAT-M1,Online");
    if (line == nullptr) {
        return false;
    }

    // Take the last four fields: RSRQ, RSRP, RSSI, RSSNR
    const char* end = strchr(line, '\r');
    if (end == nullptr) {
        end = line + strlen(line);
    }
    const char* fields[4];      // fields[0] = RSSNR ... fields[3] = RSRQ
    int found = 0;
    for (const char* p = end - 1; p > line && found < 4; p--) {
        if (*p == ',') {
            fields[found++] = p + 1;
        }
    }
    if (found < 4) {
        return false;
    }

    *rsrp = atoi(fields[2]);
    *sinr = 2 * atoi(fields[0]) - 20;   // SINR = 2 * RSSNR - 20 (SIM7080 AT manual, AT+CPSI)
    return true;
}

// ----------------------------------------------------------------------------
// Transmission
// ----------------------------------------------------------------------------

static bool lteTransmit(const uint8_t* data, size_t length) {
    #if DEBUG_SERIAL
        Serial.print("TX (");
        Serial.print(length);
        Serial.print(" B): ");
        printHex(data, length);
    #endif

    char command[32];
    snprintf(command, sizeof(command), "AT+CASEND=0,%u", (unsigned)length);

    // Module answers with ">" prompt, then expects exactly 'length' raw bytes
    sendCommand(command);
    if (!waitFor(">", 2000)) {
        return false;
    }
    #if LTE_AT_LOG
        Serial.print(">> [");
        Serial.print(length);
        Serial.println(" raw bytes of the frame]");
    #endif
    lteSerial.write(data, length);
    return waitFor("OK\r\n", 5000);
}

bool lteSendFrame(const Frame& frame) {
    uint8_t buffer[LTE_MAX_FRAME_SIZE];

    size_t length = encodeFrame(frame, buffer, sizeof(buffer));
    if (length == 0) {
        #if DEBUG_SERIAL
            Serial.println("Frame encode error.");
        #endif
        return false;
    }
    return lteTransmit(buffer, length);
}

size_t lteReceive(uint8_t* buffer, size_t bufferSize, uint32_t timeoutMs) {
    char command[32];
    snprintf(command, sizeof(command), "AT+CARECV=0,%u", (unsigned)bufferSize);

    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        // Answer: +CARECV: <length>,<raw data> ... OK   (+CARECV: 0 when nothing received)
        sendCommand(command);
        if (waitFor("+CARECV: ", AT_TIMEOUT_MS)) {
            int length = readNumber(AT_TIMEOUT_MS);
            size_t received = 0;
            if (length > 0 && (size_t)length <= bufferSize) {
                received = lteSerial.readBytes(buffer, length);
            }
            waitFor("OK\r\n", AT_TIMEOUT_MS);

            if (length > 0 && received == (size_t)length) {
                #if DEBUG_SERIAL
                    Serial.print("RX (");
                    Serial.print(length);
                    Serial.print(" B): ");
                    printHex(buffer, length);
                #endif
                return length;
            }
        }
        delay(RX_POLL_INTERVAL_MS);
    }
    return 0;
}

// Waits for the ACK of 'frame'. Datagrams that are not a matching ACK
// (e.g. a late ACK of an older frame) are skipped.
static bool receiveAck(const Frame& frame, AckPayload* outAck) {
    uint8_t buffer[64];
    uint32_t start = millis();

    while (millis() - start < LTE_ACK_TIMEOUT_MS) {
        uint32_t remainingMs = LTE_ACK_TIMEOUT_MS - (millis() - start);
        size_t length = lteReceive(buffer, sizeof(buffer), remainingMs);

        if (length > 0 && checkAck(buffer, length, frame, outAck)) {
            #if DEBUG_SERIAL
                Serial.print("ACK after ");
                Serial.print(millis() - start);
                Serial.println(" ms");
            #endif
            return true;
        }
    }

    #if DEBUG_SERIAL
        Serial.println("ACK timeout.");
    #endif
    return false;
}

bool lteSendFrameWithAck(const Frame& frame, AckPayload* outAck) {
    AckPayload ack;

    for (uint8_t attempt = 1; attempt <= LTE_MAX_ATTEMPTS; attempt++) {
        #if DEBUG_SERIAL
            Serial.print("LTE attempt ");
            Serial.print(attempt);
            Serial.print("/");
            Serial.print(LTE_MAX_ATTEMPTS);
            Serial.print(", SEQ=");
            Serial.println(frame.sequence);
        #endif

        if (!lteSendFrame(frame)) {
            #if DEBUG_SERIAL
                Serial.println("TX error - reopening UDP socket.");
            #endif
            openSocket();
            continue;
        }

        if (receiveAck(frame, &ack)) {
            if (outAck != nullptr) *outAck = ack;
            return true;
        }
    }
    return false;
}
