#include "lora_manager.h"
#include "config.h"
#include <SPI.h>
#include <RadioLib.h>

static SX1262 radio = new Module(PIN_NSS, PIN_DIO1, PIN_RST, PIN_BUSY, SPI);

// Sum of Time-on-Air of all transmissions since last loraTakeTxAirtimeUs() call
static uint32_t txAirtimeUs = 0;

// Set by DIO1 interrupt when TX or RX operation finishes
static volatile bool operationDone = false;
#if DEBUG_TX_TIME
    static volatile uint32_t isrCycles = 0;
#endif

static void onDio1ISR() {
    #if DEBUG_TX_TIME
        isrCycles = DWT->CYCCNT;
    #endif
    operationDone = true;
}

// Busy-waits for the DIO1 interrupt. Returns false on timeout.
static bool waitForOperation(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (!operationDone) {
        if (millis() - start > timeoutMs) {
            return false;
        }
    }
    return true;
}

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

#if DEBUG_TX_TIME
static double cyclesToUs(uint32_t cycles) {
    return (double)cycles * 1000000.0 / (double)HAL_RCC_GetHCLKFreq();
}
#endif

// ----------------------------------------------------------------------------

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
        LORA_TX_POWER,
        LORA_PREAMBLE,
        LORA_TCXO_VOLTAGE,
        LORA_USE_LDO
    );

    if (state != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.print("SX1262 init failed, code: ");
            Serial.println(state);
        #endif
        return false;
    }

    // Wio-SX1262 RF switch: DIO2 enables TX path (RadioLib default),
    // RF_SW pin must be HIGH during RX (datasheet, pin 1). RadioLib drives it automatically.
    radio.setRfSwitchPins(PIN_RF_SW, RADIOLIB_NC);

    radio.setDio1Action(onDio1ISR);
    radio.sleep(true);

    #if DEBUG_SERIAL
        Serial.println("SX1262 initialized.");
    #endif
    return true;
}

void loraSleep() {
    radio.sleep(true);
}

uint32_t loraTakeTxAirtimeUs() {
    uint32_t airtime = txAirtimeUs;
    txAirtimeUs = 0;
    return airtime;
}

uint32_t loraTimeOnAirUs(size_t frameLength) {
    // Calculated from config.h parameters - no SPI access, radio may stay asleep
    DataRate_t dataRate = {};
    dataRate.lora.spreadingFactor = LORA_SPREADING;
    dataRate.lora.bandwidth       = LORA_BANDWIDTH;
    dataRate.lora.codingRate      = LORA_CODING_RATE;

    PacketConfig_t packet = {};
    packet.lora.preambleLength = LORA_PREAMBLE;
    packet.lora.implicitHeader = false;
    packet.lora.crcEnabled     = true;
    // RadioLib enables low data rate optimization when symbol time >= 16 ms
    packet.lora.ldrOptimize    = ((float)(1UL << LORA_SPREADING) / LORA_BANDWIDTH) >= 16.0f;

    return (uint32_t)radio.calculateTimeOnAir(RADIOLIB_MODEM_LORA, dataRate, packet, frameLength);
}

// Transmits raw bytes and waits for TxDone.
static bool loraTransmit(const uint8_t* data, size_t length) {
    #if DEBUG_SERIAL
        Serial.print("TX (");
        Serial.print(length);
        Serial.print(" B): ");
        printHex(data, length);
    #endif

    // Wake up SX1262 and let TCXO stabilize before timing measurement
    radio.standby(RADIOLIB_SX126X_STANDBY_XOSC);

    #if DEBUG_TX_TIME
        RadioLibTime_t toaUs = radio.getTimeOnAir(length);
        Serial.print("Time-on-Air (theoretical): ");
        Serial.print(toaUs / 1000.0, 3);
        Serial.print(" ms, duty-cycle cooldown: ");
        Serial.print(toaUs * (100.0 / LORA_DUTY_CYCLE_PERCENT - 1.0) / 1000.0, 3);
        Serial.println(" ms");
    #endif

    operationDone = false;

    #if DEBUG_TX_TIME
        uint32_t t0 = DWT->CYCCNT;
    #endif
    int16_t state = radio.startTransmit(data, length);  // SPI: load buffer + start TX
    #if DEBUG_TX_TIME
        uint32_t t1 = DWT->CYCCNT;
    #endif

    if (state == RADIOLIB_ERR_NONE) {
        txAirtimeUs += (uint32_t)radio.getTimeOnAir(length);   // counted for duty cycle
    }

    if (state == RADIOLIB_ERR_NONE && !waitForOperation(TX_TIMEOUT_MS)) {
        state = RADIOLIB_ERR_TX_TIMEOUT;
    }

    #if DEBUG_TX_TIME
        uint32_t t2 = isrCycles;
        uint32_t t3 = DWT->CYCCNT;
    #endif
    int16_t finishState = radio.finishTransmit();       // SPI: clear IRQ, go to standby
    #if DEBUG_TX_TIME
        uint32_t t4 = DWT->CYCCNT;
    #endif

    if (state == RADIOLIB_ERR_NONE) {
        state = finishState;
    }

    if (state != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.print("TX error, code: ");
            Serial.println(state);
        #endif
        return false;
    }

    #if DEBUG_TX_TIME
        Serial.print("MCU SPI setup: ");
        Serial.print(cyclesToUs(t1 - t0), 2);
        Serial.print(" us, RF TX (DIO1): ");
        Serial.print(cyclesToUs(t2 - t1) / 1000.0, 3);
        Serial.print(" ms, MCU cleanup: ");
        Serial.print(cyclesToUs(t4 - t3), 2);
        Serial.println(" us");
    #endif

    return true;
}

bool loraSendFrame(const Frame& frame) {
    uint8_t buffer[LORA_MAX_PHY_SIZE];

    size_t length = encodeFrame(frame, buffer, sizeof(buffer));
    if (length == 0) {
        #if DEBUG_SERIAL
            Serial.print("Frame encode error (payload ");
            Serial.print(frame.payloadLength);
            Serial.print(" B, max ");
            Serial.print(LORA_MAX_PAYLOAD_SIZE);
            Serial.println(" B).");
        #endif
        return false;
    }

    return loraTransmit(buffer, length);
}

size_t loraReceive(uint8_t* buffer, size_t bufferSize, uint32_t timeoutMs, LoRaRxInfo* outInfo) {
    operationDone = false;

    if (radio.startReceive() != RADIOLIB_ERR_NONE) {
        return 0;
    }

    if (!waitForOperation(timeoutMs)) {
        radio.standby();
        return 0;   // timeout
    }

    size_t length = radio.getPacketLength();
    if (length == 0 || length > bufferSize) {
        radio.standby();
        return 0;
    }

    int16_t state = radio.readData(buffer, length);
    if (outInfo != nullptr) {
        outInfo->rssi = radio.getRSSI();
        outInfo->snr  = radio.getSNR();
    }
    radio.standby();

    if (state != RADIOLIB_ERR_NONE) {
        #if DEBUG_SERIAL
            Serial.print("RX error, code: ");
            Serial.println(state);
        #endif
        return 0;
    }

    #if DEBUG_SERIAL
        Serial.print("RX (");
        Serial.print(length);
        Serial.print(" B): ");
        printHex(buffer, length);
    #endif
    return length;
}

// Receives one packet and checks that it is a valid ACK for the given frame.
static bool receiveAck(const Frame& frame, AckPayload* outAck, LoRaRxInfo* outRxInfo) {
    uint8_t buffer[LORA_MAX_PHY_SIZE];

    size_t length = loraReceive(buffer, sizeof(buffer), LORA_ACK_TIMEOUT_MS, outRxInfo);
    if (length == 0) {
        #if DEBUG_SERIAL
            Serial.println("ACK timeout.");
        #endif
        return false;
    }

    if (!checkAck(buffer, length, frame, outAck)) {
        #if DEBUG_SERIAL
            Serial.println("Received frame is not a valid ACK.");
        #endif
        return false;
    }
    return true;
}

bool loraSendFrameWithAck(const Frame& frame, AckPayload* outAck, LoRaRxInfo* ackRxInfo) {
    AckPayload ack;
    LoRaRxInfo rxInfo;
    bool success = false;

    for (uint8_t attempt = 1; attempt <= LORA_MAX_ATTEMPTS && !success; attempt++) {
        #if DEBUG_SERIAL
            Serial.print("Attempt ");
            Serial.print(attempt);
            Serial.print("/");
            Serial.print(LORA_MAX_ATTEMPTS);
            Serial.print(", SEQ=");
            Serial.println(frame.sequence);
        #endif

        if (loraSendFrame(frame)) {
            success = receiveAck(frame, &ack, &rxInfo);
        }
    }

    loraSleep();

    if (success) {
        if (outAck != nullptr)    *outAck = ack;
        if (ackRxInfo != nullptr) *ackRxInfo = rxInfo;
    }
    return success;
}
