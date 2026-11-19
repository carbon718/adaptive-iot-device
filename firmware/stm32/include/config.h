#pragma once
#include <Arduino.h>

// ========================================
// FEATURE FLAGS
// ========================================
#define ENABLE_LORA             1
#define ENABLE_LTE              1
#define DEBUG_SERIAL            1
#define DEBUG_TX_TIME           0   // TX timing measurements (DWT cycle counter)
#define DEBUG_LTE_AT            0   // print every AT command and module response

// ========================================
// COMMUNICATION FRAME (docs/03-frame-structure.md)
// ========================================
#define FRAME_MAGIC             0xAA55
#define DEVICE_ID               0x0001

#define FRAME_TYPE_DATA         0x01
#define FRAME_TYPE_ACK          0x02

#define FRAME_HEADER_SIZE       11  // MAGIC(2) + DEVICE_ID(2) + TYPE(1) + SEQ(4) + LEN(2)
#define FRAME_CRC_SIZE          2
#define FRAME_OVERHEAD          (FRAME_HEADER_SIZE + FRAME_CRC_SIZE)

// ========================================
// ACK PAYLOAD
// ========================================
#define ACK_PAYLOAD_SIZE        4   // STATUS + INTERFACE + RSSI + SNR

#define ACK_STATUS_OK           0x00
#define ACK_STATUS_CRC_ERROR    0x01
#define ACK_STATUS_OTHER_ERROR  0x02

#define ACK_INTERFACE_LORA      0x01
#define ACK_INTERFACE_LTE       0x02

#define ACK_SIGNAL_NONE         ((int8_t)0x80)  // RSSI/SNR not available (LTE-M)

// ========================================
// LORA SETTINGS (Wio-SX1262)
// ========================================
#define LORA_FREQUENCY          868.0   // MHz
#define LORA_BANDWIDTH          125.0   // kHz
#define LORA_SPREADING          9
#define LORA_CODING_RATE        5
#define LORA_SYNC_WORD          0x12    // private network
#define LORA_TX_POWER           14       // dBm (max 22)
#define LORA_PREAMBLE           8
#define LORA_TCXO_VOLTAGE       3.0     // V, supplied by DIO3, must be >= 200 mV below VCC
#define LORA_USE_LDO            false   // false = DC-DC (Wio-SX1262 uses DC-DC)

#define LORA_DUTY_CYCLE_PERCENT 1       // EU 868 MHz (g1 sub-band): max 1% airtime

#define LORA_MAX_PHY_SIZE      255                                 // SX1262 limit
#define LORA_MAX_PAYLOAD_SIZE   (LORA_MAX_PHY_SIZE - FRAME_OVERHEAD) // 242 B

// ========================================
// LTE-M SETTINGS (SIM7080G, M5Stamp CAT-M)
// ========================================
#define LTE_BAUDRATE            115200
#define LTE_APN                 "iot"           // Orange IoT
#define LTE_SERVER_HOST         "iot.bcnx.eu"
#define LTE_SERVER_PORT         7000

#define LTE_MAX_FRAME_SIZE      1460                                  // AT+CASEND limit
#define LTE_MAX_PAYLOAD_SIZE    (LTE_MAX_FRAME_SIZE - FRAME_OVERHEAD) // 1447 B

#define LTE_REGISTER_TIMEOUT_MS 120000  // network registration at start-up
#define LTE_ACK_TIMEOUT_MS      5000    // UDP round trip incl. RRC connection set-up

// ========================================
// PROTOCOL TIMING & RETRIES
// ========================================
// #define MESSAGE_INTERVAL_MS     40000
#define MESSAGE_INTERVAL_MS     15000
#define TX_TIMEOUT_MS           5000    // LoRa TxDone timeout
#define LORA_ACK_TIMEOUT_MS     3000
#define LORA_MAX_ATTEMPTS       1       // Reason: LoRa is blocked by duty cycle after each TX -> fallback to LTE-M
#define LTE_MAX_ATTEMPTS        3

// ========================================
// ADAPTIVE LINK SELECTION (docs/10-adaptive-alghorithm.md)
// ========================================
#define LORA_RSSI_MIN           -120            // dBm, from last ACK (measured by Peer B)
#define LORA_SNR_MIN            -7.5f           // dB, 5 dB margin above SF9 demodulation limit
#define LORA_ACK_MAX_AGE_MS     (10UL * 60 * 1000)  // older link quality is treated as unknown
#define LORA_MAX_FAILS          3               // consecutive LoRa TX without ACK -> LoRa suspended
#define LORA_PROBE_EVERY        10              // while suspended, try LoRa every N cycles
#define LTE_RSRP_MIN            -120            // dBm

// ========================================
// NUCLEO-L476RG <-> WIO-SX1262 PINS
// ========================================
#define PIN_SCK                 PA5
#define PIN_MISO                PA6
#define PIN_MOSI                PA7
#define PIN_NSS                 PB6
#define PIN_RST                 PA9
#define PIN_BUSY                PC7
#define PIN_DIO1                PA8
#define PIN_RF_SW               PB10    // HIGH = RX path (TX path is driven by DIO2)

// ========================================
// NUCLEO-L476RG <-> M5STAMP CAT-M (UART4)
// ========================================
#define PIN_LTE_TX              PA0     // STM32 TX -> module RX
#define PIN_LTE_RX              PA1     // module TX -> STM32 RX
