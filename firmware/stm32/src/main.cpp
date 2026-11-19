// Peer A - every MESSAGE_INTERVAL_MS sends a DATA frame over the link chosen by
// the adaptive algorithm (docs/10-adaptive-alghorithm.md) and waits for ACK:
//   LoRa  -> Peer B,
//   LTE-M -> UDP server (server/udp_server.py).
// Between transmissions SX1262 is in sleep mode and STM32 in deep sleep (STOP mode).
#include <Arduino.h>
#include <STM32RTC.h>
#include <STM32LowPower.h>
#include "config.h"
#include "modules/frame.h"
#include "modules/lora_manager.h"
#include "modules/lte_manager.h"
#include "modules/link_selector.h"

// ---- Test traffic, simulation ----
#define TEST_FRAME_SIZE         64                                // whole frame [B]
#define TEST_PAYLOAD_SIZE       (TEST_FRAME_SIZE - FRAME_OVERHEAD)  // TEST_FRAME_SIZE - 13 B
#define MAX_DELAY_NORMAL_MS     60000   // regular frame - can wait for LoRa
#define MAX_DELAY_URGENT_MS     2000    // urgent frame - faster than LoRa delivery (~3.2 s)
#define TEST_URGENT_EVERY       5       // every 5th frame is urgent

static_assert(TEST_FRAME_SIZE >= FRAME_OVERHEAD, "TEST_FRAME_SIZE must be >= FRAME_OVERHEAD");
static_assert(TEST_PAYLOAD_SIZE <= LTE_MAX_PAYLOAD_SIZE, "Payload too large for LTE-M");

static STM32RTC& rtc = STM32RTC::getInstance();

// Frame waiting for a free link (kept when no link was available)
static uint8_t  payload[TEST_PAYLOAD_SIZE];
static uint32_t sequenceNumber = 0;
static bool     framePending = false;

static void lowPowerInit() {
    // RTC clocked from the 32.768 kHz LSE crystal - keeps running in STOP mode,
    // wakes up the MCU and is the time base of the link selection algorithm.
    rtc.setClockSource(STM32RTC::LSE_CLOCK);
    rtc.begin();
    LowPower.begin();
}

// Puts STM32 into STOP mode for the rest of MESSAGE_INTERVAL_MS.
// LoRa duty cycle is enforced by the link selector (LoRa blocked after each TX).
static void sleepUntilNextMessage(uint32_t activeStartMs) {
    uint32_t activeMs = millis() - activeStartMs;
    uint32_t sleepMs  = (activeMs < MESSAGE_INTERVAL_MS) ? (MESSAGE_INTERVAL_MS - activeMs) : 0;

    #if DEBUG_SERIAL
        Serial.print("Active: ");
        Serial.print(activeMs);
        Serial.print(" ms, deep sleep: ");
        Serial.print(sleepMs);
        Serial.println(" ms");
        Serial.println("----------------------------------------");
        Serial.flush();     // finish UART transmission before entering STOP mode
    #endif

    if (sleepMs > 0) {
        LowPower.deepSleep(sleepMs);    // execution continues here after wake-up (no reset)
    }
}

// Deterministic pseudo-random payload (fixed seed) for repeatable tests
static void generateTestPayload(uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (uint8_t)random(0, 256);
    }
}

static void sendTestFrame() {
    // New frame only when the previous one was not left waiting for a link
    if (!framePending) {
        generateTestPayload(payload, sizeof(payload));
        sequenceNumber++;
    }

    Frame frame;
    frame.deviceId      = DEVICE_ID;
    frame.type          = FRAME_TYPE_DATA;
    frame.sequence      = sequenceNumber;
    frame.payload       = payload;
    frame.payloadLength = sizeof(payload);

    uint32_t maxDelayMs = (sequenceNumber % TEST_URGENT_EVERY == 0) ? MAX_DELAY_URGENT_MS
                                                                    : MAX_DELAY_NORMAL_MS;

    LinkResult result = linkSendFrame(frame, maxDelayMs);
    framePending = (result == LINK_NO_LINK);

    #if DEBUG_SERIAL
        switch (result) {
            case LINK_DELIVERED_LORA: Serial.println("Delivered via LoRa.");                 break;
            case LINK_DELIVERED_LTE:  Serial.println("Delivered via LTE-M.");                break;
            case LINK_NOT_DELIVERED:  Serial.println("NOT delivered (no ACK) - frame lost."); break;
            case LINK_NO_LINK:        Serial.println("No link available - frame waits.");    break;
        }
    #endif
}

void setup() {
    #if DEBUG_SERIAL
        Serial.begin(115200);
    #endif
    delay(15000);   // time to re-flash before the MCU enters deep sleep

    randomSeed(12345);

    // On-board LED (LD2) off - no unnecessary current draw
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    #if DEBUG_TX_TIME
        // Enable DWT cycle counter for precise timing measurements
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    #endif

    lowPowerInit();     // RTC first - time base of the link selector

    // A failed interface is not fatal - the algorithm uses the other one
    bool loraReady = ENABLE_LORA && loraInit();
    bool lteReady  = ENABLE_LTE && lteInit();
    linkInit(loraReady, lteReady);

    #if DEBUG_SERIAL
        Serial.print("LoRa ready: ");
        Serial.print(loraReady);
        Serial.print(", LTE-M ready: ");
        Serial.println(lteReady);
    #endif
}

void loop() {
    uint32_t activeStartMs = millis();
    sendTestFrame();
    sleepUntilNextMessage(activeStartMs);
}
