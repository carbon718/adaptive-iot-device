#pragma once
#include <Arduino.h>

#define ENABLE_LORA             1
#define ENABLE_LTE              0
#define ENABLE_GENERATOR        1
#define DEBUG_SERIAL            1
#define DEBUG_TX_TIME           1

// ========================================
// PROTOCOL AND FRAME SETTINGS
// ========================================
#define FRAME_MAGIC             0xAA55
#define DEVICE_ID               0x0001
#define FRAME_HEADER_SIZE       10
#define FRAME_CRC_SIZE          2
#define FRAME_OVERHEAD          (FRAME_HEADER_SIZE + FRAME_CRC_SIZE)
#define FRAME_MAX_SIZE          512
#define FRAME_MAX_PAYLOAD_SIZE  (FRAME_MAX_SIZE - FRAME_OVERHEAD)

// ========================================
// LORA SETTINGS
// ========================================
#define LORA_FREQUENCY          868.0
#define LORA_BANDWIDTH          125.0
#define LORA_SPREADING          9
#define LORA_CODING_RATE        5
#define LORA_SYNC_WORD          0x12
#define LORA_TX_POWER           2
#define LORA_PREAMBLE           8
#define LORA_TCXO               3.0
#define LORA_USE_DCDC           false

// LoRa PHY layer maximum packet size (in bytes)
#define LORA_MAX_PHY_SIZE       255

// ========================================
// PROTOCOL TIMING & RETRIES
// ========================================
#define MESSAGE_INTERVAL_MS     40000
#define ACK_TIMEOUT_MS          3000
#define MAX_RETRIES             3

// ========================================
// NUCLEO-L476RG & WIO-SX1262 PINS
// ========================================
#define PIN_SCK                 PA5
#define PIN_MISO                PA6
#define PIN_MOSI                PA7
#define PIN_NSS                 PB6
#define PIN_RST                 PA9
#define PIN_BUSY                PC7
#define PIN_DIO1                PA8
#define PIN_RF_SW               PB10