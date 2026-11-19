#pragma once
#include <Arduino.h>
#include "frame.h"

// Signal parameters of the last received LoRa packet
struct LoRaRxInfo {
    float rssi;     // dBm
    float snr;      // dB
};

// Initializes SX1262 and starts continuous reception.
bool loraInit();

// Non-blocking. If a packet has been received, copies it into buffer and returns
// its length. Returns 0 when nothing was received (or on error).
size_t loraReceive(uint8_t* buffer, size_t bufferSize, LoRaRxInfo* outInfo);

// Encodes the frame, transmits it and returns to continuous reception.
bool loraSendFrame(const Frame& frame);
