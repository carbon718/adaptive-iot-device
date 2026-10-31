#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// Raspberry Pi Pico - SPI0
#define PIN_MISO   16
#define PIN_MOSI   19
#define PIN_SCK    18

// Wio-SX1262
#define PIN_RF_SW  15
#define PIN_NSS    17
#define PIN_RST    20
#define PIN_BUSY   21
#define PIN_DIO1   22

// Parametry LoRa
constexpr float LORA_FREQUENCY = 868.0;
constexpr float LORA_BANDWIDTH = 125.0;
constexpr uint8_t LORA_SF = 7;
constexpr uint8_t LORA_CR = 5;
constexpr int8_t LORA_POWER = 10;
constexpr uint16_t LORA_PREAMBLE = 8;

// NSS, DIO1, RESET, BUSY
SX1262 radio = new Module(
    PIN_NSS,
    PIN_DIO1,
    PIN_RST,
    PIN_BUSY,
    SPI
);

uint32_t packetCounter = 0;


void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== SX1262 DIAGNOSTIC ===");

    // Ustawienia GPIO
    pinMode(PIN_NSS, OUTPUT);
    digitalWrite(PIN_NSS, HIGH);

    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, HIGH);

    pinMode(PIN_BUSY, INPUT);
    pinMode(PIN_DIO1, INPUT);

    pinMode(PIN_RF_SW, OUTPUT);
    digitalWrite(PIN_RF_SW, LOW);

    // SPI0
    SPI.setRX(PIN_MISO);
    SPI.setTX(PIN_MOSI);
    SPI.setSCK(PIN_SCK);
    SPI.begin();

    delay(100);

    Serial.println("SPI initialized");

    // Ręczny reset SX1262
    Serial.println("Resetting SX1262...");

    digitalWrite(PIN_RST, LOW);
    delay(100);

    digitalWrite(PIN_RST, HIGH);
    delay(200);

    Serial.print("BUSY after reset: ");
    Serial.println(digitalRead(PIN_BUSY));

    Serial.print("MISO idle: ");
    Serial.println(digitalRead(PIN_MISO));

    Serial.println("Calling radio.begin...");

    int16_t state = radio.begin(
        868.0,
        125.0,
        7,
        5,
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
        10,
        8,
        1.8,
        false
    );

    Serial.print("RadioLib result: ");
    Serial.println(state);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("SUCCESS: SX1262 DETECTED!");
    } else {
        Serial.println("FAILED: SX1262 NOT DETECTED");
    }
}

void loop() {
    delay(1000);
}

