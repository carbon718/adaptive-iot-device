#include "frame_decoder.h"
#include "../../../stm32/include/config.h"

static inline uint16_t readBE16(const uint8_t* buf) {
    return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

static inline uint32_t readBE32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           (uint32_t)buf[3];
}

// Calculate CRC-16 (polynomial 0x8005 / reflected 0xA001, init 0xFFFF)
uint16_t calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool decodeFrame(
    const uint8_t* inFrame,
    size_t frameLength,
    DecodedFrame* outFrame
) {
    if (inFrame == nullptr || outFrame == nullptr) {
        return false;
    }

    if (frameLength < FRAME_OVERHEAD || frameLength > FRAME_MAX_SIZE) {
        return false;
    }

    // HEADER (10 Bytes, Big Endian)
    outFrame->magic         = readBE16(&inFrame[0]); // 0..1: MAGIC (2B)
    outFrame->deviceId      = readBE16(&inFrame[2]); // 2..3: DEVICE ID (2B)
    outFrame->sequence      = readBE32(&inFrame[4]); // 4..7: SEQUENCE (4B)
    outFrame->payloadLength = readBE16(&inFrame[8]); // 8..9: PAYLOAD LENGTH (2B)

    if (outFrame->magic != FRAME_MAGIC) {
        return false;
    }

    size_t expectedSize = FRAME_HEADER_SIZE + outFrame->payloadLength + FRAME_CRC_SIZE;
    if (frameLength != expectedSize) {
        return false;
    }

    // PAYLOAD
    outFrame->payload = (outFrame->payloadLength > 0) ? &inFrame[FRAME_HEADER_SIZE] : nullptr;

    // CRC-16 (2 Bytes, Big Endian)
    size_t crcIndex = FRAME_HEADER_SIZE + outFrame->payloadLength;
    outFrame->crc = readBE16(&inFrame[crcIndex]);

    uint16_t calculatedCrc = calculateCRC16(inFrame, crcIndex);
    if (outFrame->crc != calculatedCrc) {
        return false;
    }

    return true;
}