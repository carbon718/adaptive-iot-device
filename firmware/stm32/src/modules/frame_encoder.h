#pragma once
#include <Arduino.h>


uint16_t calculateCRC16(const uint8_t* data, size_t length);

void generatePseudoRandomPayload(uint8_t* buffer, size_t length);

size_t encodeFrame(
    uint32_t sequence,
    const uint8_t* payload,
    uint16_t payloadLength,
    uint8_t* outFrame
);