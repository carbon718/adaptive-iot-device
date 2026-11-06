#include "lora_manager.h"
#include "config.h"
#include <SPI.h>
#include <RadioLib.h>


static SX1262 radio = new Module(
    PIN_NSS,
    PIN_DIO1,
    PIN_RST,
    PIN_BUSY,
    SPI
);

static uint32_t packetCounter = 0;

bool loraInit(){
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
            Serial.println("Failed to initialize LoRa SX1262module.");
        #endif
        return false;
    }

    #if DEBUG_SERIAL
        Serial.println("LoRa SX1262 module has been initialized.");
    #endif
    return true;
}

bool loraSendMessage(const uint8_t* frame, size_t length){
    /*
        @TODO 
        Implement the function to send a message using the LoRa module. This function should handle the transmission of the provided frame of data with the specified length.
        It should return true if the message was sent successfully, and false otherwise. Consider implementing error handling and retries as necessary.
    */
}