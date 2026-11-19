#pragma once
#include <Arduino.h>
#include "frame.h"

// Adaptive link selection between LoRa and LTE-M (docs/10-adaptive-alghorithm.md)

enum LinkResult {
    LINK_NO_LINK,           // no interface available - frame was not sent
    LINK_NOT_DELIVERED,     // sent, but no ACK on any interface
    LINK_DELIVERED_LORA,
    LINK_DELIVERED_LTE
};

// Remembers which interfaces initialized correctly. Requires running RTC (time base).
void linkInit(bool loraReady, bool lteReady);

// Chooses the link (availability, required delivery time, energy), sends the frame
// and falls back to the other link when no ACK is received.
LinkResult linkSendFrame(const Frame& frame, uint32_t maxDelayMs);
