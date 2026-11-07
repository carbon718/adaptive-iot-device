#include "frame_encoder.h"
#include "config.h"

static inline void writeBE16(uint8_t* buf, uint16_t val) {
    buf[0] = (uint8_t)((val >> 8) & 0xFF);
    buf[1] = (uint8_t)(val & 0xFF);
}

static inline void writeBE32(uint8_t* buf, uint32_t val) {
    buf[0] = (uint8_t)((val >> 24) & 0xFF);
    buf[1] = (uint8_t)((val >> 16) & 0xFF);
    buf[2] = (uint8_t)((val >> 8) & 0xFF);
    buf[3] = (uint8_t)(val & 0xFF);
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

void generatePseudoRandomPayload(uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = random(0, 256);
    }
}

size_t encodeFrame(
    uint32_t sequence,
    const uint8_t* payload,
    uint16_t payloadLength,
    uint8_t* outFrame
) {
    size_t totalSize = FRAME_HEADER_SIZE + FRAME_CRC_SIZE + payloadLength;

    if (outFrame == nullptr || totalSize > FRAME_MAX_SIZE) {
        return 0;
    }

    if (payloadLength > 0 && payload == nullptr) {
        return 0;
    }

    // HEADER (10 Bytes, Big Endian)
    writeBE16(&outFrame[0], FRAME_MAGIC);     // 0..1: MAGIC (2B)
    writeBE16(&outFrame[2], DEVICE_ID);       // 2..3: DEVICE ID (2B)
    writeBE32(&outFrame[4], sequence);        // 4..7: SEQUENCE (4B)
    writeBE16(&outFrame[8], payloadLength);   // 8..9: PAYLOAD LENGTH (2B)

    // PAYLOAD (payloadLength Bytes)
    if (payloadLength > 0) {
        memcpy(&outFrame[FRAME_HEADER_SIZE], payload, payloadLength);
    }

    // CRC-16 (2 Bytes, Big Endian)
    uint16_t crc = calculateCRC16(outFrame, FRAME_HEADER_SIZE + payloadLength);
    writeBE16(&outFrame[FRAME_HEADER_SIZE + payloadLength], crc);

    return totalSize;
}