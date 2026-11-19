#pragma once
#include <Arduino.h>
#include "frame.h"

// Configures SIM7080G (APN, CAT-M), waits for network registration,
// activates PDP context and opens a UDP socket to LTE_SERVER_HOST:LTE_SERVER_PORT.
bool lteInit();

// Current link state: returns true if registered in the network (AT+CEREG?) and camped
// on a CAT-M cell; then fills RSRP [dBm] and SINR [dB] read with AT+CPSI?.
bool lteGetSignal(int* rsrp, int* sinr);

// Encodes the frame and sends it once as a UDP datagram (no ACK).
bool lteSendFrame(const Frame& frame);

// Waits up to timeoutMs for one UDP datagram. Returns number of received bytes, 0 on timeout.
size_t lteReceive(uint8_t* buffer, size_t bufferSize, uint32_t timeoutMs);

// Sends a DATA frame and waits for a matching ACK (same SEQUENCE), up to MAX_RETRIES attempts.
bool lteSendFrameWithAck(const Frame& frame, AckPayload* outAck);
