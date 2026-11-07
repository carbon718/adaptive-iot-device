#include "lora_manager.h"
#include "config.h"
#include <SPI.h>
#include <RadioLib.h>

#define LORA_DUTY_CYCLE 0.01f

static SX1262 radio = new Module(
    PIN_NSS,
    PIN_DIO1,
    PIN_RST,
    PIN_BUSY,
    SPI
);

// Flag for DIO1 interrupt (TxDone) - always required for non-blocking TX
static volatile bool txDoneFlag = false;

#if DEBUG_TX_TIME
    static volatile uint32_t isrTimestampCycles = 0;

    #define MEASUREMENT_COUNT 1000

    struct TxMeasurement {
        uint32_t sequence;
        uint32_t mcuStartUs;
        uint32_t rfTxUs;
        uint32_t mcuFinishUs;
        uint32_t totalOpUs;
        int16_t txStatus;
        int16_t sleepStatus;
    };

    static TxMeasurement measurements[MEASUREMENT_COUNT];
    static uint32_t measurementCount = 0;
#endif

// Interrupt Service Routine (ISR) triggered when RF transmission finishes
void onTxDoneISR() {
    #if DEBUG_TX_TIME
        isrTimestampCycles = DWT->CYCCNT;
    #endif
    txDoneFlag = true;
}

bool loraInit() {
    pinMode(PIN_RF_SW, OUTPUT);
    pinMode(PIN_NSS, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_BUSY, INPUT);
    pinMode(PIN_DIO1, INPUT);

    digitalWrite(PIN_NSS, HIGH);
    digitalWrite(PIN_RST, HIGH);

    SPI.begin();

    #if DEBUG_SERIAL
        Serial.println("SPI has been initialized.");
    #endif

    int16_t state = radio.begin(
        LORA_FREQUENCY,
        LORA_BANDWIDTH,
        LORA_SPREADING,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_TX_POWER,
        LORA_PREAMBLE,
        LORA_TCXO,
        LORA_USE_DCDC
    );

    #if DEBUG_SERIAL
        Serial.print("radio.begin(): ");
        Serial.println(state);
    #endif

    if (state != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.println("Failed to initialize LoRa SX1262 module.");
        #endif
        return false;
    }

    // Set the DIO1 interrupt handler for TxDone event
    radio.setDio1Action(onTxDoneISR);

    #if DEBUG_SERIAL
        Serial.println("LoRa SX1262 module has been initialized.");
    #endif

    return true;
}

bool loraSendMessage(const uint8_t* frame, size_t length) {
    // 1. Validate buffer pointer and minimum frame size (Header + CRC = 12 B)
    if (frame == nullptr || length < FRAME_OVERHEAD) {
        #if DEBUG_SERIAL
            Serial.println("LoRa TX Error: Empty buffer or frame smaller than header!");
        #endif
        return false;
    }

    // 2. Validate size against the technological limit of LoRa PHY (255 B for SX1262)
    if (length > LORA_MAX_PHY_SIZE) {
        #if DEBUG_SERIAL
            Serial.print("LoRa PHY Error: Frame too large (");
            Serial.print(length);
            Serial.print(" B > max ");
            Serial.print(LORA_MAX_PHY_SIZE);
            Serial.println(" B)!");
        #endif
        return false;
    }

    #if DEBUG_SERIAL
        Serial.print("Sending LoRa frame (");
        Serial.print(length);
        Serial.println(" B):");
        for (size_t i = 0; i < length; i++) {
            if (frame[i] < 0x10) Serial.print("0");
            Serial.print(frame[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    #endif
    
    // Wake up SX1262 from sleep mode
    int16_t wakeState = radio.standby();
    if (wakeState != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.print("LoRa wake-up error: ");
            Serial.println(wakeState);
        #endif
        return false;
    }

    // Enable the RF switch before transmission
    digitalWrite(PIN_RF_SW, HIGH);
    delay(2);

    #if DEBUG_TX_TIME
        RadioLibTime_t toaUs = radio.getTimeOnAir(length);
        Serial.print("Theoretical RF Time-on-Air: ");
        Serial.print(toaUs);
        Serial.print(" us (");
        Serial.print(toaUs / 1000.0f, 3);
        Serial.println(" ms)");

        // Calculate the minimum TX cooldown time required to maintain the configured RF duty cycle
        uint64_t minWaitUs = (uint64_t)ceil(
            (double)toaUs * ((1.0 / LORA_DUTY_CYCLE) - 1.0)
        );
        Serial.print("Minimum TX cooldown time: ");
        Serial.print((double)minWaitUs / 1000.0, 3);
        Serial.println(" ms");
    #endif

    // Reset interrupt flag before starting transmission
    txDoneFlag = false;

    // --- STAGE 1: Load frame over SPI and start transmitter ---
    #if DEBUG_TX_TIME
        uint32_t t0 = DWT->CYCCNT;
    #endif
    int16_t txState = radio.startTransmit(const_cast<uint8_t*>(frame), length);
    #if DEBUG_TX_TIME
        uint32_t t1 = DWT->CYCCNT;
    #endif

    if (txState == RADIOLIB_ERR_NONE) {
        // --- STAGE 2: Actual over-the-air transmission (wait for DIO1 interrupt) ---
        // 5-second safety timeout in case the interrupt does not fire
        uint32_t waitStartMs = millis();
        while (!txDoneFlag) {
            if (millis() - waitStartMs > 5000) {
                txState = RADIOLIB_ERR_TX_TIMEOUT;
                break;
            }
        }
        #if DEBUG_TX_TIME
            uint32_t t2 = isrTimestampCycles;
        #endif

        // --- STAGE 3: Finish transmission (clear IRQ flags over SPI) ---
        #if DEBUG_TX_TIME
            uint32_t t3 = DWT->CYCCNT;
        #endif
        int16_t finishState = radio.finishTransmit();
        #if DEBUG_TX_TIME
            uint32_t t4 = DWT->CYCCNT;
        #endif

        if (txState == RADIOLIB_ERR_NONE) {
            txState = finishState;
        }

        #if DEBUG_TX_TIME
            uint32_t cpuFreq = HAL_RCC_GetHCLKFreq();
            float cyclesToUs = 1000000.0f / (float)cpuFreq;

            float mcuStartUs  = (float)(t1 - t0) * cyclesToUs; // SPI setup time (startTransmit)
            float rfTxTimeUs  = (float)(t2 - t1) * cyclesToUs; // Over-the-air TX time (until DIO1 ISR)
            float mcuFinishUs = (float)(t4 - t3) * cyclesToUs; // SPI cleanup time (finishTransmit)
            float mcuTotalUs  = mcuStartUs + mcuFinishUs;      // Total MCU active SPI time
            float totalOpUs   = (float)(t4 - t0) * cyclesToUs; // Total operation time (equivalent to blocking radio.transmit)

            Serial.print("MCU busy time (SPI setup + cleanup): ");
            Serial.print(mcuTotalUs, 3);
            Serial.print(" us (setup: ");
            Serial.print(mcuStartUs, 2);
            Serial.print(" us, cleanup: ");
            Serial.print(mcuFinishUs, 2);
            Serial.println(" us)");

            Serial.print("Actual RF transmission time (DIO1):  ");
            Serial.print(rfTxTimeUs, 3);
            Serial.print(" us (");
            Serial.print(rfTxTimeUs / 1000.0f, 3);
            Serial.println(" ms)");

            Serial.print("Actual total TX operation time:      ");
            Serial.print(totalOpUs, 3);
            Serial.print(" us (");
            Serial.print(totalOpUs / 1000.0f, 3);
            Serial.println(" ms)");
        #endif
    }
    
    // Set the RF switch back to low state after transmission is complete
    digitalWrite(PIN_RF_SW, LOW);
    delay(2);
    
    // Put SX1262 into sleep mode (retain configuration in warm start)
    int16_t sleepState = radio.sleep(true);
    #if DEBUG_SERIAL
        Serial.print("SX1262 sleep(): ");
        Serial.println(sleepState);
    #endif

    if (txState != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.print("LoRa TX Error: ");
            Serial.println(txState);
        #endif
        return false;
    }

    #if DEBUG_SERIAL
        Serial.println("LoRa frame has been sent successfully.");
        Serial.println("----------------------------------------");
    #endif

    return true;
}