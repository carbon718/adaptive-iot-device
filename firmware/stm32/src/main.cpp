#include <Arduino.h>
#include "config.h"
#include "modules/lora_manager.h"
#include "modules/frame_encoder.h"

#define TEST_DATA_SIZE 30

static uint32_t packetCounter = 0;

void sendTestLoraFrame() {
    uint8_t payload[TEST_DATA_SIZE-FRAME_OVERHEAD];
    uint8_t frameBuffer[FRAME_MAX_SIZE];

    packetCounter++;

    // Generate a pseudo-random payload for testing purposes
    generatePseudoRandomPayload(payload, sizeof(payload));

    // Enoce frame returns the length of the encoded frame (10B header, 32B payload, 2B CRC = 44 B), or 0 if an error occurred
    size_t frameLength = encodeFrame(
        packetCounter,
        payload,
        sizeof(payload),
        frameBuffer
    );

    if (frameLength == 0) {
        #if DEBUG
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

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    #if ENABLE_LORA
        if (!loraInit()) {
            while (true) {
                delay(1000);
            }
        }
    #endif
}

void loop() {
    static unsigned long previousTime = 0;

    if (millis() - previousTime >= MESSAGE_INTERVAL_MS) {
        previousTime = millis();
        sendTestLoraFrame();
    }
}