#include <Arduino.h>
#include "config.h"
#include "modules/lora_manager.h"
#include "modules/frame_encoder.h"

#define TEST_DATA_SIZE 30
static_assert(TEST_DATA_SIZE >= FRAME_OVERHEAD, "TEST_DATA_SIZE must be >= FRAME_OVERHEAD (12 bytes)");

static uint32_t packetCounter = 0;

void sendTestLoraFrame() {
    uint8_t payload[TEST_DATA_SIZE - FRAME_OVERHEAD];
    uint8_t frameBuffer[FRAME_MAX_SIZE];

    packetCounter++;

    // Generate a deterministic pseudo-random payload for repeatable testing
    generatePseudoRandomPayload(payload, sizeof(payload));

    // Encode frame returns the total length of the encoded frame 
    // (10B header + payload + 2B CRC = TEST_DATA_SIZE B), or 0 if an error occurred
    size_t frameLength = encodeFrame(
        packetCounter,
        payload,
        sizeof(payload),
        frameBuffer
    );

    if (frameLength == 0) {
        #if DEBUG_SERIAL
            Serial.println("Error encoding frame.");
        #endif
        return;
    }

    // Send the encoded frame over LoRa
    loraSendMessage(frameBuffer, frameLength);
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    // Fixed seed ensures repeatable payload sequences across different SF/BW test runs
    randomSeed(12345);

    #if DEBUG_TX_TIME
        // Enable ARM Cortex-M4 DWT cycle counter for cycle-accurate timing measurements
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    #endif

    #if ENABLE_LORA
        if (!loraInit()) {
            while (true) {
                delay(1000);
            }
        }
    #endif
}

void loop() {
    // Initialize so the first packet is transmitted immediately upon startup
    static unsigned long previousTime = 0;
    static bool firstRun = true;

    unsigned long currentMillis = millis();
    if (firstRun || (currentMillis - previousTime >= MESSAGE_INTERVAL_MS)) {
        firstRun = false;
        previousTime = currentMillis;
        sendTestLoraFrame();
    }
}