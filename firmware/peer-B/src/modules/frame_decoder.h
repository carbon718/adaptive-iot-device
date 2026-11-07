#pragma once

#include <Arduino.h>

struct DecodedFrame {
    uint16_t magic;
    uint16_t deviceId;
    uint32_t sequence;
    uint16_t payloadLength;
    const uint8_t* payload;
    uint16_t crc;
};

uint16_t calculateCRC16(const uint8_t* data, size_t length);

bool decodeFrame(
    const uint8_t* inFrame,
    size_t frameLength,
    DecodedFrame* outFrame
);