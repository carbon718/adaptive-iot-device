#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// NUCLEO-L476RG - SPI1
#define PIN_SCK     PA5
#define PIN_MISO    PA6
#define PIN_MOSI    PA7

// Wio-SX1262
#define PIN_NSS     PB6
#define PIN_RST     PA9
#define PIN_BUSY    PC7
#define PIN_DIO1    PA8
#define PIN_RF_SW   PB10

// RadioLib: NSS, DIO1, RESET, BUSY
SX1262 radio = new Module(
    PIN_NSS,
    PIN_DIO1,
    PIN_RST,
    PIN_BUSY,
    SPI
);

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("================================");
    Serial.println("NUCLEO-L476RG + Wio-SX1262");
    Serial.println("Test inicjalizacji");
    Serial.println("================================");

    // GPIO
    pinMode(PIN_RF_SW, OUTPUT);
    pinMode(PIN_NSS, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_BUSY, INPUT);
    pinMode(PIN_DIO1, INPUT);

    digitalWrite(PIN_NSS, HIGH);
    digitalWrite(PIN_RST, HIGH);

    // RF_SW - stan zalezy od polaryzacji modulu
    digitalWrite(PIN_RF_SW, LOW);

    // SPI1: PA5, PA6, PA7
    SPI.begin();

    Serial.println("SPI uruchomione.");

    Serial.print("BUSY przed resetem: ");
    Serial.println(digitalRead(PIN_BUSY));

    // Reset sprzetowy
    Serial.println("Reset SX1262...");

    digitalWrite(PIN_RST, LOW);
    delay(2);

    digitalWrite(PIN_RST, HIGH);
    delay(20);

    Serial.print("BUSY po resecie: ");
    Serial.println(digitalRead(PIN_BUSY));

    Serial.println("Wywoluje radio.begin(868 MHz)...");

    // Europejskie pasmo LoRa
    int16_t state = radio.begin(
      868.0,     // frequency [MHz]
      125.0,     // bandwidth [kHz]
      9,         // spreading factor
      7,         // coding rate 4/7
      0x12,      // sync word
      10,        // output power [dBm]
      8,         // preamble length
      3.0,       // TCXO voltage - do weryfikacji
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
        Serial.println("Czestotliwosc: 868 MHz");
        Serial.println("================================");
    }
    else
    {
        Serial.println();
        Serial.println("BLAD INICJALIZACJI SX1262");

        if (state == RADIOLIB_ERR_CHIP_NOT_FOUND)
        {
            Serial.println("RadioLib nie wykrywa ukladu.");
            Serial.println("Sprawdz polaczenia:");
            Serial.println("3V3, GND");
            Serial.println("SCK, MISO, MOSI");
            Serial.println("NSS, RESET, BUSY");
        }
        else
        {
            Serial.print("Kod bledu RadioLib: ");
            Serial.println(state);
        }
    }
}

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
