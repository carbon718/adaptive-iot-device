#pragma once
#include <Arduino.h>

// Communication Frame (see docs/03-frame-structure.md)
// | MAGIC 2B | DEVICE_ID 2B | FRAME_TYPE 1B | SEQUENCE 4B | PAYLOAD_LENGTH 2B | PAYLOAD N B | CRC-16 2B |
struct Frame {
    uint16_t       deviceId;
    uint8_t        type;           // FRAME_TYPE_DATA / FRAME_TYPE_ACK
    uint32_t       sequence;
    const uint8_t* payload;        // points to payload bytes (not copied)
    uint16_t       payloadLength;
};

// ACK payload (4 B)
struct AckPayload {
    uint8_t status;                // ACK_STATUS
    uint8_t interface;             // ACK_INTERFACE
    int8_t  rssi;                  // dBm, ACK_SIGNAL_NONE for LTE-M
    int8_t  snr;                   // dB,  ACK_SIGNAL_NONE for LTE-M
};

enum FrameResult {
    FRAME_OK = 0,
    FRAME_ERR_LENGTH,
    FRAME_ERR_MAGIC,
    FRAME_ERR_CRC
};

// CRC-16/MODBUS (poly 0x8005 reflected 0xA001, init 0xFFFF)
uint16_t crc16Modbus(const uint8_t* data, size_t length);

// Serializes frame into buffer. Returns frame length in bytes, or 0 on error.
size_t encodeFrame(const Frame& frame, uint8_t* outBuffer, size_t bufferSize);

// Parses and validates a received buffer. outFrame->payload points into buffer.
FrameResult decodeFrame(const uint8_t* buffer, size_t length, Frame* outFrame);

// ACK payload <-> 4 bytes
void encodeAckPayload(const AckPayload& ack, uint8_t* outBuffer);
bool decodeAckPayload(const uint8_t* buffer, uint16_t length, AckPayload* outAck);
