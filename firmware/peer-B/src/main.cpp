#include <Arduino.h>
#include "../../stm32/include/config.h"
#include "modules/lora_manager.h"
#include "modules/frame_decoder.h"

static void printHexBytes(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void receiveLoraFrame() {
    uint8_t frameBuffer[LORA_MAX_PHY_SIZE];
    size_t frameLength = 0;
    LoRaPacketStats loraStats;

    if (!loraReceiveMessage(frameBuffer, sizeof(frameBuffer), &frameLength, &loraStats)) {
        return;
    }

    #if DEBUG_SERIAL
        Serial.println("========================================");
        Serial.print("Received LoRa PHY frame (");
        Serial.print(frameLength);
        Serial.println(" B):");
        printHexBytes(frameBuffer, frameLength);

        Serial.println("--- LoRa PHY & RF Parameters ---");
        Serial.print("Channel Config:     ");
        Serial.print(LORA_FREQUENCY, 1);
        Serial.print(" MHz | BW: ");
        Serial.print(LORA_BANDWIDTH, 1);
        Serial.print(" kHz | SF: ");
        Serial.print(LORA_SPREADING);
        Serial.print(" | CR: 4/");
        Serial.print(LORA_CODING_RATE);
        Serial.print(" | SyncWord: 0x");
        Serial.println(LORA_SYNC_WORD, HEX);

        Serial.print("Packet RSSI:        ");
        Serial.print(loraStats.rssi, 2);
        Serial.println(" dBm");

        Serial.print("Instant/Signal RSSI:");
        Serial.print(loraStats.signalRssi, 2);
        Serial.println(" dBm");

        Serial.print("Packet SNR:         ");
        Serial.print(loraStats.snr, 2);
        Serial.print(" dB (Demod margin: +");
        Serial.print(loraStats.snrMarginDb, 2);
        Serial.println(" dB)");

        Serial.print("Est. Path Loss:     ");
        Serial.print(loraStats.estimatedPathLoss, 2);
        Serial.print(" dB (assuming TX power = ");
        Serial.print(LORA_TX_POWER);
        Serial.println(" dBm)");

        Serial.print("Frequency Error:    ");
        Serial.print(loraStats.freqErrorHz, 2);
        Serial.print(" Hz (");
        Serial.print(loraStats.freqErrorPpm, 3);
        Serial.println(" ppm)");

        Serial.print("Theoretical ToA:    ");
        Serial.print(loraStats.timeOnAirUs);
        Serial.print(" us (");
        Serial.print((double)loraStats.timeOnAirUs / 1000.0, 3);
        Serial.println(" ms)");

        Serial.print("Effective Bitrate:  ");
        Serial.print(loraStats.effectiveBitrateBps, 2);
        Serial.println(" bps");
    #endif

    DecodedFrame decoded;
    if (!decodeFrame(frameBuffer, frameLength, &decoded)) {
        #if DEBUG_SERIAL
            Serial.println("--- Protocol Frame Data ---");
            Serial.println("Error decoding frame (invalid header or CRC mismatch)!");
            Serial.println("========================================");
        #endif
        return;
    }

    #if DEBUG_SERIAL
        Serial.println("--- Protocol Frame Data ---");
        Serial.print("Magic Header:       0x");
        Serial.println(decoded.magic, HEX);
        Serial.print("Device ID:          0x");
        Serial.println(decoded.deviceId, HEX);
        Serial.print("Sequence Number:    ");
        Serial.println(decoded.sequence);
        Serial.print("Payload Length:     ");
        Serial.print(decoded.payloadLength);
        Serial.println(" B");
        Serial.print("Payload (HEX):      ");
        if (decoded.payloadLength > 0) {
            printHexBytes(decoded.payload, decoded.payloadLength);
        } else {
            Serial.println("<empty>");
        }
        Serial.print("CRC-16 (Valid):     0x");
        Serial.println(decoded.crc, HEX);
        Serial.println("========================================");
    #endif
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    #if ENABLE_LORA
        if (!loraInit()) {
            while (true) {
                delay(1000);
            }
        }
    #endif
}

void loop() {
    receiveLoraFrame();
}