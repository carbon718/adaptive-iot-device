
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ========================================
// RASPBERRY PI PICO - SPI0
// ========================================

#define PIN_SCK   10
#define PIN_MISO  12
#define PIN_MOSI  11

// ========================================
// WIO-SX1262
// ========================================
#define PIN_NSS   13
#define PIN_RST   14
#define PIN_BUSY  15
#define PIN_DIO1  8
#define PIN_RF_SW 7

// ========================================
// RADIOLIB
// Module(NSS, DIO1, RESET, BUSY)
// ========================================

SX1262 radio = new Module(
    PIN_NSS,
    PIN_DIO1,
    PIN_RST,
    PIN_BUSY,
    SPI1
);

// ========================================
// SETUP
// ========================================

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("================================");
    Serial.println("RASPBERRY PI PICO + Wio-SX1262");
    Serial.println("Test inicjalizacji Peer B");
    Serial.println("================================");

    // GPIO
    pinMode(PIN_RF_SW, OUTPUT);
    pinMode(PIN_NSS, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_BUSY, INPUT);
    pinMode(PIN_DIO1, INPUT);

    digitalWrite(PIN_NSS, HIGH);
    digitalWrite(PIN_RST, HIGH);

    // RF_SW - stan spoczynkowy
    digitalWrite(PIN_RF_SW, LOW);

    // SPI0
    SPI1.setSCK(PIN_SCK);
    SPI1.setRX(PIN_MISO);
    SPI1.setTX(PIN_MOSI);
    SPI1.begin();

    Serial.println("SPI0 uruchomione.");

    Serial.print("BUSY przed resetem: ");
    Serial.println(digitalRead(PIN_BUSY));

    // Reset sprzętowy SX1262
    Serial.println("Reset SX1262...");

    digitalWrite(PIN_RST, LOW);
    delay(2);

    digitalWrite(PIN_RST, HIGH);
    delay(20);

    Serial.print("BUSY po resecie: ");
    Serial.println(digitalRead(PIN_BUSY));

    // Inicjalizacja LoRa
    Serial.println("Wywoluje radio.begin(868 MHz)...");

    int16_t state = radio.begin(
        868.0,     // frequency [MHz]
        125.0,     // bandwidth [kHz]
        9,         // spreading factor
        7,         // coding rate 4/7
        0x12,      // sync word
        10,        // output power [dBm]
        8,         // preamble length
        3.0,       // TCXO voltage [V]
        false      // DC-DC regulator
    );

    Serial.print("radio.begin() zwrocilo: ");
    Serial.println(state);

    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println();
        Serial.println("================================");
        Serial.println("SX1262 ZAINICJALIZOWANY!");
        Serial.println("Komunikacja SPI dziala.");
        Serial.println("Peer B gotowy.");
        Serial.println("Czestotliwosc: 868 MHz");
        Serial.println("================================");
    }
    else
    {
        Serial.println();
        Serial.println("BLAD INICJALIZACJI SX1262");

        Serial.print("Kod bledu RadioLib: ");
        Serial.println(state);
    }
}

// ========================================
// LOOP
// ========================================

void loop()
{
    static unsigned long previousTime = 0;

    if (millis() - previousTime >= 2000)
    {
        previousTime = millis();

        Serial.print("BUSY=");
        Serial.print(digitalRead(PIN_BUSY));

        Serial.print("  DIO1=");
        Serial.println(digitalRead(PIN_DIO1));
    }
}
