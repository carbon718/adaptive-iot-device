#include "link_selector.h"
#include "config.h"
#include "lora_manager.h"
#include "lte_manager.h"
#include <STM32RTC.h>

enum Link { LINK_NONE, LINK_LORA, LINK_LTE };

// Interfaces initialized at start-up
static bool loraReady = false;
static bool lteReady  = false;

// LoRa state
static uint64_t loraBlockedUntilMs = 0;     // end of duty-cycle blocking
static bool     loraHasAck   = false;       // at least one ACK received
static int8_t   loraRssi     = 0;           // from last ACK (measured by Peer B)
static int8_t   loraSnr      = 0;
static uint64_t loraAckTimeMs = 0;
static uint8_t  loraFailCount = 0;          // consecutive LoRa TX without ACK
static uint32_t loraSuspendedCycles = 0;    // cycles since LoRa was suspended

// Time from RTC [ms] - keeps running in deep sleep (millis() does not)
static uint64_t nowMs() {
    uint32_t subSeconds = 0;
    time_t epoch = STM32RTC::getInstance().getEpoch(&subSeconds);
    return (uint64_t)epoch * 1000 + subSeconds;
}

void linkInit(bool loraOk, bool lteOk) {
    loraReady = loraOk;
    lteReady  = lteOk;
}

// ----------------------------------------------------------------------------
// Sending over a single link + state update (step 5)
// ----------------------------------------------------------------------------

static bool sendOverLora(const Frame& frame) {
    AckPayload ack;
    LoRaRxInfo ackRx;
    bool delivered = loraSendFrameWithAck(frame, &ack, &ackRx);

    // Duty cycle 1%: after ToA the channel is blocked for ToA x 99
    uint32_t airtimeUs = loraTakeTxAirtimeUs();
    loraBlockedUntilMs = nowMs() + (uint64_t)airtimeUs * (100 / LORA_DUTY_CYCLE_PERCENT - 1) / 1000;

    if (delivered) {
        loraFailCount = 0;
        loraHasAck    = true;
        loraRssi      = ack.rssi;
        loraSnr       = ack.snr;
        loraAckTimeMs = nowMs();
    } else if (loraFailCount < 255) {
        loraFailCount++;
    }
    return delivered;
}

static bool sendOverLte(const Frame& frame) {
    AckPayload ack;
    return lteSendFrameWithAck(frame, &ack);
}

#if DEBUG_SERIAL
static const char* linkName(Link link) {
    return link == LINK_LORA ? "LoRa" : (link == LINK_LTE ? "LTE-M" : "none");
}
#endif

// ----------------------------------------------------------------------------
// Algorithm
// ----------------------------------------------------------------------------

LinkResult linkSendFrame(const Frame& frame, uint32_t maxDelayMs) {
    uint64_t now = nowMs();

    // Step 1 + 6: LoRa availability. When suspended after LORA_MAX_FAILS,
    // LoRa is still tried every LORA_PROBE_EVERY cycles (recovery probe).
    bool loraSuspended = loraFailCount >= LORA_MAX_FAILS;
    bool loraProbe = false;
    if (loraSuspended) {
        loraSuspendedCycles++;
        loraProbe = (loraSuspendedCycles % LORA_PROBE_EVERY) == 0;
    } else {
        loraSuspendedCycles = 0;
    }

    bool loraAvailable = loraReady
                         && now >= loraBlockedUntilMs
                         && (!loraSuspended || loraProbe)
                         && frame.payloadLength <= LORA_MAX_PAYLOAD_SIZE;

    // Step 2: LTE-M availability (current registration state and signal)
    int lteRsrp = 0;
    int lteSinr = 0;
    bool lteAvailable = lteReady
                        && lteGetSignal(&lteRsrp, &lteSinr)
                        && lteRsrp >= LTE_RSRP_MIN;

    // Step 3: LoRa link quality from the last ACK
    bool loraGood;
    if (!loraHasAck || now - loraAckTimeMs > LORA_ACK_MAX_AGE_MS) {
        loraGood = true;    // no recent data - try LoRa (optimistic)
    } else {
        loraGood = loraRssi >= LORA_RSSI_MIN && loraSnr >= LORA_SNR_MIN;
    }

    // Step 4: link choice
    uint32_t loraMaxDeliveryMs = loraTimeOnAirUs(FRAME_OVERHEAD + frame.payloadLength) / 1000
                                 + LORA_ACK_TIMEOUT_MS;
    Link link;
    if (!loraAvailable && !lteAvailable) {
        link = LINK_NONE;
    } else if (loraAvailable && loraGood && maxDelayMs >= loraMaxDeliveryMs) {
        link = LINK_LORA;       // cheaper in energy and fast enough
    } else if (lteAvailable) {
        link = LINK_LTE;        // LoRa blocked, weak, too slow or frame too large
    } else {
        link = LINK_LORA;       // the only available link, even with weak quality
    }

    #if DEBUG_SERIAL
        Serial.print("SEQ=");
        Serial.print(frame.sequence);
        Serial.print(" maxDelay=");
        Serial.print(maxDelayMs);
        Serial.print(" ms | LoRa: avail=");
        Serial.print(loraAvailable);
        Serial.print(" blocked=");
        Serial.print(now < loraBlockedUntilMs ? (uint32_t)((loraBlockedUntilMs - now) / 1000) : 0);
        Serial.print(" s fails=");
        Serial.print(loraFailCount);
        Serial.print(loraProbe ? " (probe)" : "");
        Serial.print(" good=");
        Serial.print(loraGood);
        Serial.print(" RSSI=");
        Serial.print(loraRssi);
        Serial.print(" SNR=");
        Serial.print(loraSnr);
        Serial.print(" delivery=");
        Serial.print(loraMaxDeliveryMs);
        Serial.print(" ms | LTE-M: avail=");
        Serial.print(lteAvailable);
        Serial.print(" RSRP=");
        Serial.print(lteRsrp);
        Serial.print(" SINR=");
        Serial.print(lteSinr);
        Serial.print(" | -> ");
        Serial.println(linkName(link));
    #endif

    // Step 5: send and fall back to the other link when there is no ACK
    if (link == LINK_NONE) {
        return LINK_NO_LINK;
    }

    if (link == LINK_LORA) {
        if (sendOverLora(frame)) {
            return LINK_DELIVERED_LORA;
        }
        if (lteAvailable) {
            #if DEBUG_SERIAL
                Serial.println("No LoRa ACK - fallback to LTE-M.");
            #endif
            if (sendOverLte(frame)) {
                return LINK_DELIVERED_LTE;
            }
        }
        return LINK_NOT_DELIVERED;
    }

    // link == LINK_LTE
    if (sendOverLte(frame)) {
        return LINK_DELIVERED_LTE;
    }
    if (loraAvailable) {
        #if DEBUG_SERIAL
            Serial.println("No LTE-M ACK - fallback to LoRa.");
        #endif
        if (sendOverLora(frame)) {
            return LINK_DELIVERED_LORA;
        }
    }
    return LINK_NOT_DELIVERED;
}
