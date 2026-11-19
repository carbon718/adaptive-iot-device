#include "frame.h"
#include "config.h"

static void writeBE16(uint8_t* buf, uint16_t val) {
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val);
}

static void writeBE32(uint8_t* buf, uint32_t val) {
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val);
}

static uint16_t readBE16(const uint8_t* buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

static uint32_t readBE32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |  (uint32_t)buf[3];
}

uint16_t crc16Modbus(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

size_t encodeFrame(const Frame& frame, uint8_t* outBuffer, size_t bufferSize) {
    size_t totalSize = FRAME_OVERHEAD + frame.payloadLength;

    if (outBuffer == nullptr || totalSize > bufferSize) {
        return 0;
    }
    if (frame.payloadLength > 0 && frame.payload == nullptr) {
        return 0;
    }

    // HEADER (11 B, Big Endian)
    writeBE16(&outBuffer[0], FRAME_MAGIC);          // 0..1  MAGIC
    writeBE16(&outBuffer[2], frame.deviceId);       // 2..3  DEVICE_ID
    outBuffer[4] = frame.type;                      // 4     FRAME_TYPE
    writeBE32(&outBuffer[5], frame.sequence);       // 5..8  SEQUENCE
    writeBE16(&outBuffer[9], frame.payloadLength);  // 9..10 PAYLOAD_LENGTH

    // PAYLOAD
    if (frame.payloadLength > 0) {
        memcpy(&outBuffer[FRAME_HEADER_SIZE], frame.payload, frame.payloadLength);
    }

    // CRC-16 over HEADER + PAYLOAD
    size_t crcOffset = FRAME_HEADER_SIZE + frame.payloadLength;
    writeBE16(&outBuffer[crcOffset], crc16Modbus(outBuffer, crcOffset));

    return totalSize;
}

FrameResult decodeFrame(const uint8_t* buffer, size_t length, Frame* outFrame) {
    if (buffer == nullptr || outFrame == nullptr || length < FRAME_OVERHEAD) {
        return FRAME_ERR_LENGTH;
    }
    if (readBE16(&buffer[0]) != FRAME_MAGIC) {
        return FRAME_ERR_MAGIC;
    }

    uint16_t payloadLength = readBE16(&buffer[9]);
    if (length != (size_t)FRAME_OVERHEAD + payloadLength) {
        return FRAME_ERR_LENGTH;
    }

    size_t crcOffset = FRAME_HEADER_SIZE + payloadLength;
    if (readBE16(&buffer[crcOffset]) != crc16Modbus(buffer, crcOffset)) {
        return FRAME_ERR_CRC;
    }

    outFrame->deviceId      = readBE16(&buffer[2]);
    outFrame->type          = buffer[4];
    outFrame->sequence      = readBE32(&buffer[5]);
    outFrame->payloadLength = payloadLength;
    outFrame->payload       = &buffer[FRAME_HEADER_SIZE];
    return FRAME_OK;
}

void encodeAckPayload(const AckPayload& ack, uint8_t* outBuffer) {
    outBuffer[0] = ack.status;
    outBuffer[1] = ack.interface;
    outBuffer[2] = (uint8_t)ack.rssi;
    outBuffer[3] = (uint8_t)ack.snr;
}

bool decodeAckPayload(const uint8_t* buffer, uint16_t length, AckPayload* outAck) {
    if (buffer == nullptr || outAck == nullptr || length != ACK_PAYLOAD_SIZE) {
        return false;
    }
    outAck->status    = buffer[0];
    outAck->interface = buffer[1];
    outAck->rssi      = (int8_t)buffer[2];
    outAck->snr       = (int8_t)buffer[3];
    return true;
}
