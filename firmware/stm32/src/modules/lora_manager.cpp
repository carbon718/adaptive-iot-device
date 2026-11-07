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

    #if DEBUG_SERIAL
        Serial.println("LoRa SX1262 module has been initialized.");
    #endif

    return true;
}

bool loraSendMessage(const uint8_t* frame, size_t length) {
    // 1. Walidacja bufora i minimalnego rozmiaru ramki (Nagłówek + CRC = 12 B)
    if (frame == nullptr || length < FRAME_OVERHEAD) {
        #if DEBUG_SERIAL
            Serial.println("LoRa TX Error: Empty buffer or frame smaller than header!");
        #endif
        return false;
    }

    // Validation of the size against the technological limit of LoRa PHY (255 B for SX1262)
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
            Serial.print(frame[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    #endif
    
    // Wake up SX1262 from sleep
    int16_t wakeState = radio.standby();

    if (wakeState != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.print("LoRa wake-up error: ");
            Serial.println(wakeState);
        #endif
        return false;
    }


    // Set the RF switch to high before transmission
    digitalWrite(PIN_RF_SW, HIGH);
    delay(2);

    #if DEBUG_TX_TIME
        RadioLibTime_t toaUs = radio.getTimeOnAir(length);

        Serial.print("Theoretical RF Time-on-Air: ");
        Serial.print(toaUs);
        Serial.print(" us (");
        Serial.print(toaUs / 1000.0f, 3);
        Serial.println(" ms)");

        // Calculate the minimum TX cooldown time required  to maintain the configured RF duty cycle.
        uint64_t minWaitUs = (uint64_t)ceil(
            (double)toaUs * ((1.0 / LORA_DUTY_CYCLE) - 1.0)
        );

        Serial.print("Minimum TX cooldown time: ");
        Serial.print((double)minWaitUs / 1000.0, 3);
        Serial.println(" ms");

        uint32_t startCycles = DWT->CYCCNT;
    #endif

    // Transmission of the raw byte buffer (RadioLib accepts uint8_t* and length)
    int16_t txState = radio.transmit(const_cast<uint8_t*>(frame), length);
    
    #if DEBUG_TX_TIME
        uint32_t elapsedCycles = DWT->CYCCNT - startCycles;

        // Calculate total device-side TX operation time in microseconds.
        // Includes SPI communication, radio setup, RF transmission and waiting for the TX completion event.
        uint32_t cpuFreq = HAL_RCC_GetHCLKFreq();
        float txOperationUs = (float)elapsedCycles * 1000000.0f / cpuFreq;

        Serial.print("Actual TX operation time: ");
        Serial.print(txOperationUs, 3);
        Serial.print(" us (");
        Serial.print(txOperationUs / 1000.0f, 3);
        Serial.println(" ms)");
    #endif
    
    // Return the RF switch to the low state after transmission is complete
    digitalWrite(PIN_RF_SW, LOW);
    delay(2);
    
    // Enter SX1262 sleep mode (retain configuration)
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
    #endif

    return true;
}