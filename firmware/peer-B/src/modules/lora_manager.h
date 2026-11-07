#pragma once

#include <Arduino.h>

struct LoRaPacketStats {
    float rssi;               // Average packet RSSI [dBm]
    float signalRssi;         // Estimated LoRa signal power after despreading [dBm]
    float snr;                // Signal-to-Noise Ratio [dB]
    float freqErrorHz;        // Carrier frequency offset [Hz]
    float freqErrorPpm;       // Carrier frequency offset [ppm]
    float snrMarginDb;        // Margin above theoretical SF demodulation floor [dB]
    float estimatedPathLoss;  // Estimated RF path loss (TX Power - RSSI) [dB]
    uint32_t timeOnAirUs;     // Theoretical Time-on-Air for received packet length [us]
    float effectiveBitrateBps;// Effective PHY bitrate for this frame [bps]
};

bool loraInit();
bool loraReceiveMessage(
    uint8_t* outBuffer,
    size_t maxLength,
    size_t* outLength,
    LoRaPacketStats* outStats
);