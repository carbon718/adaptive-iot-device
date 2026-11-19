// Peer B - continuously listens on LoRa. For each valid DATA frame from Peer A
// it sends back an ACK (same DEVICE_ID and SEQUENCE) containing RSSI and SNR
// of the received packet. Powered from USB - no power saving.
#include <Arduino.h>
#include "config.h"
#include "modules/frame.h"
#include "modules/lora_manager.h"

// Short pause before replying, so that Peer A has time to switch from TX to RX
#define ACK_DELAY_MS    20

static void printHex(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

// Rounds RSSI/SNR to a single signed byte (-128 = 0x80 is reserved for "no value")
static int8_t toSignalByte(float value) {
    return (int8_t)constrain(lroundf(value), -127, 127);
}

static bool sendAck(const Frame& data, const LoRaRxInfo& rx) {
    AckPayload ack;
    ack.status    = ACK_STATUS_OK;
    ack.interface = ACK_INTERFACE_LORA;
    ack.rssi      = toSignalByte(rx.rssi);
    ack.snr       = toSignalByte(rx.snr);

    uint8_t payload[ACK_PAYLOAD_SIZE];
    encodeAckPayload(ack, payload);

    Frame frame;
    frame.deviceId      = data.deviceId;
    frame.type          = FRAME_TYPE_ACK;
    frame.sequence      = data.sequence;
    frame.payload       = payload;
    frame.payloadLength = sizeof(payload);

    delay(ACK_DELAY_MS);
    return loraSendFrame(frame);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Peer B - LoRa receiver");

    if (!loraInit()) {
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    uint8_t buffer[LORA_MAX_PHY_SIZE];
    LoRaRxInfo rx;

    size_t length = loraReceive(buffer, sizeof(buffer), &rx);
    if (length == 0) {
        return;     // nothing received
    }

    Frame frame;
    FrameResult result = decodeFrame(buffer, length, &frame);

    // Reply only to a valid DATA frame. Otherwise no ACK - Peer A will retry.
    bool ackSent = false;
    if (result == FRAME_OK && frame.type == FRAME_TYPE_DATA) {
        ackSent = sendAck(frame, rx);
    }

    // Report printed after the ACK, so that printing does not delay the reply
    Serial.print("RX (");
    Serial.print(length);
    Serial.print(" B): ");
    printHex(buffer, length);
    Serial.print("RSSI=");
    Serial.print(rx.rssi, 1);
    Serial.print(" dBm, SNR=");
    Serial.print(rx.snr, 1);
    Serial.println(" dB");

    if (result != FRAME_OK) {
        Serial.print("Frame decode error: ");
        Serial.println(result);     // 1 = length, 2 = magic, 3 = CRC
    } else if (frame.type != FRAME_TYPE_DATA) {
        Serial.println("Not a DATA frame - ignored.");
    } else {
        Serial.print("DATA from device 0x");
        Serial.print(frame.deviceId, HEX);
        Serial.print(", SEQ=");
        Serial.print(frame.sequence);
        Serial.print(", payload ");
        Serial.print(frame.payloadLength);
        Serial.print(" B -> ACK ");
        Serial.println(ackSent ? "sent" : "FAILED");
    }
    Serial.println("----------------------------------------");
}
