#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ========================================
// USTAWIENIA LORA - MUSZA BYC TAKIE SAME
// W PEER A I PEER B
// ========================================

#define LORA_FREQUENCY       868.0
#define LORA_BANDWIDTH       125.0
#define LORA_SPREADING       9
#define LORA_CODING_RATE     7
#define LORA_SYNC_WORD       0x12
#define LORA_TX_POWER        10
#define LORA_PREAMBLE        8
#define LORA_TCXO            3.0
#define LORA_USE_DCDC        false

// ========================================
// USTAWIENIA PROTOKOLU
// ========================================

#define MESSAGE_INTERVAL_MS  40000
#define ACK_TIMEOUT_MS       3000
#define MAX_RETRIES          3

#define MESSAGE_TEXT         "Wiadomosc testowa z Peer A"

// ========================================
// NUCLEO-L476RG - SPI1
// ========================================

#define PIN_SCK     PA5
#define PIN_MISO    PA6
#define PIN_MOSI    PA7

// ========================================
// WIO-SX1262
// ========================================

#define PIN_NSS     PB6
#define PIN_RST     PA9
#define PIN_BUSY    PC7
#define PIN_DIO1    PA8
#define PIN_RF_SW   PB10

// ========================================
// RADIO
// ========================================

SX1262 radio = new Module(
    PIN_NSS,
    PIN_DIO1,
    PIN_RST,
    PIN_BUSY,
    SPI
);

uint32_t packetCounter = 0;

// ========================================
// INICJALIZACJA
// ========================================

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("================================");
    Serial.println("PEER A - NADAJNIK LORA");
    Serial.println("NUCLEO-L476RG + SX1262");
    Serial.println("================================");

    pinMode(PIN_RF_SW, OUTPUT);
    pinMode(PIN_NSS, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_BUSY, INPUT);
    pinMode(PIN_DIO1, INPUT);

    digitalWrite(PIN_NSS, HIGH);
    digitalWrite(PIN_RST, HIGH);

    SPI.begin();

    Serial.println("SPI uruchomione.");

    int16_t state = radio.begin(
        LORA_FREQUENCY,
        LORA_BANDWIDTH,
        LORA_SPREADING,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_TX_POWER,
        LORA_PREAMBLE,
        LORA_TCXO,
        LORA_USE_DCDC
    );

    Serial.print("radio.begin(): ");
    Serial.println(state);

    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.println("BLAD INICJALIZACJI SX1262!");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("SX1262 GOTOWY!");
    Serial.println("PEER A rozpoczyna transmisje.");
}

// ========================================
// WYSYLANIE WIADOMOSCI I OCZEKIWANIE NA ACK
// ========================================

void sendMessage()
{
    packetCounter++;

    String packet = "DATA:";
    packet += packetCounter;
    packet += ":";
    packet += MESSAGE_TEXT;

    Serial.println();
    Serial.println("--------------------------------");
    Serial.print("Wysylam pakiet nr: ");
    Serial.println(packetCounter);

    Serial.print("Tresc: ");
    Serial.println(MESSAGE_TEXT);

    bool acknowledged = false;

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++)
    {
        Serial.println();
        Serial.print("Proba transmisji: ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.println(MAX_RETRIES);

        // Nadawanie
        digitalWrite(PIN_RF_SW, HIGH);
        delay(2);

        int16_t txState = radio.transmit(packet);

        if (txState != RADIOLIB_ERR_NONE)
        {
            Serial.print("Blad TX: ");
            Serial.println(txState);
            continue;
        }

        Serial.println("Pakiet wyslany.");

        // Oczekiwanie na ACK
        digitalWrite(PIN_RF_SW, LOW);
        delay(2);

        Serial.println("Oczekiwanie na ACK...");

        String response;

        int16_t rxState = radio.receive(
            response,
            0,
            ACK_TIMEOUT_MS
        );

        if (rxState == RADIOLIB_ERR_NONE)
        {
            String expectedACK = "ACK:";
            expectedACK += packetCounter;

            if (response == expectedACK)
            {
                Serial.println();
                Serial.println("< ACK OTRZYMANY >");

                Serial.print("Potwierdzenie: ");
                Serial.println(response);

                Serial.print("RSSI: ");
                Serial.print(radio.getRSSI());
                Serial.println(" dBm");

                Serial.print("SNR: ");
                Serial.print(radio.getSNR());
                Serial.println(" dB");

                acknowledged = true;
                break;
            }
            else
            {
                Serial.print("Otrzymano nieprawidlowe ACK: ");
                Serial.println(response);
            }
        }
        else if (rxState == RADIOLIB_ERR_RX_TIMEOUT)
        {
            Serial.println("TIMEOUT - brak ACK.");
        }
        else
        {
            Serial.print("Blad RX: ");
            Serial.println(rxState);
        }
    }

    Serial.println();

    if (acknowledged)
    {
        Serial.println("================================");
        Serial.println("WIADOMOSC DOSTARCZONA!");
        Serial.println("================================");
    }
    else
    {
        Serial.println("================================");
        Serial.println("NIE OTRZYMANO ACK!");
        Serial.println("Nie potwierdzono dostarczenia.");
        Serial.println("================================");
    }

    // Powrot do nasluchiwania przed nastepnym cyklem
    digitalWrite(PIN_RF_SW, LOW);
}

// ========================================
// LOOP
// ========================================

void loop()
{
    static unsigned long previousTime = 0;

    if (millis() - previousTime >= MESSAGE_INTERVAL_MS)
    {
        previousTime = millis();
        sendMessage();
    }
}
