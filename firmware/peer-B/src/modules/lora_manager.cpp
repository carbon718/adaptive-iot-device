#include "lora_manager.h"
#include "config.h"
#include <SPI.h>
#include <RadioLib.h>

// Peer B TX power (ACK) - overrides LORA_TX_POWER from the shared config.h
#define PEER_B_TX_POWER         2       // dBm

static SX1262 radio = new Module(PIN_NSS, PIN_DIO1, PIN_RST, PIN_BUSY, SPI);

// Set by DIO1 interrupt when a packet has been received (RxDone)
static volatile bool packetReceived = false;

static void onDio1ISR() {
    packetReceived = true;
}

bool loraInit() {
    SPI.setMISO(PIN_MISO);
    SPI.setMOSI(PIN_MOSI);
    SPI.setSCLK(PIN_SCK);
    SPI.begin();

    int16_t state = radio.begin(
        LORA_FREQUENCY,
        LORA_BANDWIDTH,
        LORA_SPREADING,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        PEER_B_TX_POWER,
        LORA_PREAMBLE,
        LORA_TCXO_VOLTAGE,
        LORA_USE_LDO
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("SX1262 init failed, code: ");
        Serial.println(state);
        return false;
    }

    // Wio-SX1262 RF switch: DIO2 enables TX path, RF_SW pin HIGH enables RX path
    radio.setRfSwitchPins(PIN_RF_SW, RADIOLIB_NC);
    radio.setDio1Action(onDio1ISR);

    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("SX1262 startReceive failed, code: ");
        Serial.println(state);
        return false;
    }

    Serial.println("SX1262 initialized, listening...");
    return true;
}

size_t loraReceive(uint8_t* buffer, size_t bufferSize, LoRaRxInfo* outInfo) {
    if (!packetReceived) {
        return 0;
    }
    packetReceived = false;

    size_t length = radio.getPacketLength();
    int16_t state = RADIOLIB_ERR_NONE;

    if (length == 0 || length > bufferSize) {
        length = 0;
    } else {
        state = radio.readData(buffer, length);
        outInfo->rssi = radio.getRSSI();
        outInfo->snr  = radio.getSNR();
    }

    radio.startReceive();   // re-arm receiver for the next packet

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("RX error, code: ");
        Serial.println(state);
        return 0;
    }
    return length;
}

bool loraSendFrame(const Frame& frame) {
    uint8_t buffer[LORA_MAX_PHY_SIZE];

    size_t length = encodeFrame(frame, buffer, sizeof(buffer));
    if (length == 0) {
        Serial.println("Frame encode error.");
        return false;
    }

    int16_t state = radio.transmit(buffer, length);   // blocking, waits for TxDone

    // TxDone also triggers DIO1 interrupt - it is not a received packet
    packetReceived = false;
    radio.startReceive();

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("TX error, code: ");
        Serial.println(state);
        return false;
    }
    return true;
}
