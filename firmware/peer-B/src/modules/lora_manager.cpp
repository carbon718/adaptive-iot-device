#include "lora_manager.h"
#include "../../../stm32/include/config.h"
#include <SPI.h>
#include <RadioLib.h>

static SX1262 radio = new Module(
    PIN_NSS,
    PIN_DIO1,
    PIN_RST,
    PIN_BUSY,
    SPI
);

// Flag for DIO1 interrupt (RxDone)
static volatile bool rxDoneFlag = false;

// Returns theoretical LoRa demodulation SNR limit [dB] for a given Spreading Factor
static float getSnrDemodulationLimit(uint8_t sf) {
    switch (sf) {
        case 5:  return -2.5f;
        case 6:  return -5.0f;
        case 7:  return -7.5f;
        case 8:  return -10.0f;
        case 9:  return -12.5f;
        case 10: return -15.0f;
        case 11: return -17.5f;
        case 12: return -20.0f;
        default: return -12.5f;
    }
}

// Interrupt Service Routine (ISR) triggered when RF packet reception finishes
void onRxDoneISR() {
    rxDoneFlag = true;
}

bool loraInit() {
    pinMode(PIN_RF_SW, OUTPUT);
    pinMode(PIN_NSS, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_BUSY, INPUT);
    pinMode(PIN_DIO1, INPUT);

    // Keep RF switch LOW for reception path
    digitalWrite(PIN_RF_SW, LOW);
    digitalWrite(PIN_NSS, HIGH);
    digitalWrite(PIN_RST, HIGH);

    SPI.setMISO(PIN_MISO);
    SPI.setMOSI(PIN_MOSI);
    SPI.setSCLK(PIN_SCK);
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

    // Set the DIO1 interrupt handler for RxDone event
    radio.setDio1Action(onRxDoneISR);

    // Put radio into continuous receive mode
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.print("Failed to start reception, code: ");
            Serial.println(state);
        #endif
        return false;
    }

    #if DEBUG_SERIAL
        Serial.println("LoRa SX1262 module initialized and listening.");
    #endif

    return true;
}

bool loraReceiveMessage(
    uint8_t* outBuffer,
    size_t maxLength,
    size_t* outLength,
    LoRaPacketStats* outStats
) {
    if (!rxDoneFlag) {
        return false;
    }

    rxDoneFlag = false;

    if (outBuffer == nullptr || outLength == nullptr) {
        radio.startReceive();
        return false;
    }

    size_t packetLength = radio.getPacketLength();
    if (packetLength == 0 || packetLength > maxLength || packetLength > LORA_MAX_PHY_SIZE) {
        #if DEBUG_SERIAL
            Serial.print("LoRa RX Error: Invalid packet size (");
            Serial.print(packetLength);
            Serial.println(" B)!");
        #endif
        radio.startReceive();
        return false;
    }

    int16_t state = radio.readData(outBuffer, packetLength);

    if (state != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.print("LoRa RX Error: ");
            Serial.println(state);
        #endif
        radio.startReceive();
        return false;
    }

    // Read SX1262 PHY packet status registers before re-arming receiver
    if (outStats != nullptr) {
        outStats->rssi              = radio.getRSSI(true);  // true = packet RSSI
        outStats->signalRssi        = radio.getRSSI(false); // false = instantaneous/signal RSSI
        outStats->snr               = radio.getSNR();
        outStats->freqErrorHz       = radio.getFrequencyError();
        outStats->freqErrorPpm      = outStats->freqErrorHz / (float)LORA_FREQUENCY;
        outStats->snrMarginDb       = outStats->snr - getSnrDemodulationLimit(LORA_SPREADING);
        outStats->estimatedPathLoss = (float)LORA_TX_POWER - outStats->rssi;
        outStats->timeOnAirUs       = (uint32_t)radio.getTimeOnAir(packetLength);

        if (outStats->timeOnAirUs > 0) {
            outStats->effectiveBitrateBps = ((float)packetLength * 8.0f * 1000000.0f) / (float)outStats->timeOnAirUs;
        } else {
            outStats->effectiveBitrateBps = 0.0f;
        }
    }

    // Immediately re-arm receiver for the next frame
    radio.startReceive();

    *outLength = packetLength;
    return true;
}