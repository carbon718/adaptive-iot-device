#pragma once
#include <Arduino.h>
#include "frame.h"

// Signal parameters of the last received LoRa packet
struct LoRaRxInfo {
    float rssi;     // dBm
    float snr;      // dB
};

bool loraInit();

// Encodes the frame and transmits it once (no ACK). Used e.g. by Peer B to send ACK.
bool loraSendFrame(const Frame& frame);

// Waits up to timeoutMs for one packet. Returns number of received bytes, 0 on timeout/error.
size_t loraReceive(uint8_t* buffer, size_t bufferSize, uint32_t timeoutMs, LoRaRxInfo* outInfo);

// Sends a DATA frame and waits for a matching ACK (same SEQUENCE), up to MAX_RETRIES attempts.
// On success fills outAck (parameters measured by the receiver) and ackRxInfo (measured by us).
bool loraSendFrameWithAck(const Frame& frame, AckPayload* outAck, LoRaRxInfo* ackRxInfo);

// Puts SX1262 into sleep mode (warm start, configuration retained)
void loraSleep();

// Returns total Time-on-Air [us] of all transmissions (incl. retries) since the last call, then resets it.
uint32_t loraTakeTxAirtimeUs();

// Theoretical Time-on-Air [us] of a frame of the given length (current LoRa settings).
uint32_t loraTimeOnAirUs(size_t frameLength);
