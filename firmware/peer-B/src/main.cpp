
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

// Numer ostatnio odebranej wiadomosci
uint32_t lastPacketId = 0;

// ========================================
// INICJALIZACJA
// ========================================

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("================================");
    Serial.println("PEER B - ODBIORNIK LORA");
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
    Serial.println("PEER B oczekuje na wiadomosci.");

    // Odbior
    digitalWrite(PIN_RF_SW, HIGH);
}

// ========================================
// ODBIOR WIADOMOSCI
// ========================================

void loop()
{
    String packet;

    // ====================================
    // TRYB ODBIORU
    // ====================================

    digitalWrite(PIN_RF_SW, HIGH);
    delay(2);

    Serial.println();
    Serial.println("Nasluchiwanie...");

    int16_t state = radio.receive(packet);

    if (state == RADIOLIB_ERR_NONE)
    {
        float rssi = radio.getRSSI();
        float snr = radio.getSNR();

        Serial.println();
        Serial.println("================================");
        Serial.println("ODEBRANO PAKIET!");
        Serial.println("================================");

        Serial.print("Tresc pakietu: ");
        Serial.println(packet);

        Serial.print("RSSI: ");
        Serial.print(rssi);
        Serial.println(" dBm");

        Serial.print("SNR: ");
        Serial.print(snr);
        Serial.println(" dB");

        // =================================
        // SPRAWDZENIE FORMATU DATA
        // DATA:numer:wiadomosc
        // =================================

        if (!packet.startsWith("DATA:"))
        {
            Serial.println("Nieznany format pakietu.");
            return;
        }

        int separator = packet.indexOf(':', 5);

        if (separator < 0)
        {
            Serial.println("Nieprawidlowy pakiet DATA.");
            return;
        }

        uint32_t packetId = packet.substring(
            5,
            separator
        ).toInt();

        String message = packet.substring(separator + 1);

        if (packetId == 0)
        {
            Serial.println("Nieprawidlowy numer pakietu.");
            return;
        }

        // =================================
        // SPRAWDZENIE DUPLIKATU
        // =================================

        if (packetId == lastPacketId)
        {
            Serial.println("To jest duplikat wiadomosci.");
            Serial.println("Ponownie wysylam ACK.");
        }
        else
        {
            lastPacketId = packetId;

            Serial.println();
            Serial.println("NOWA WIADOMOSC!");

            Serial.print("Numer pakietu: ");
            Serial.println(packetId);

            Serial.print("Wiadomosc: ");
            Serial.println(message);
        }

        // =================================
        // PRZYGOTOWANIE ACK
        // =================================

        String ack = "ACK:";
        ack += packetId;

        Serial.println();
        Serial.print("Wysylam potwierdzenie: ");
        Serial.println(ack);

        // =================================
        // TRYB NADAWANIA
        // =================================

        digitalWrite(PIN_RF_SW, LOW);
        delay(2);

        int16_t txState = radio.transmit(ack);

        if (txState == RADIOLIB_ERR_NONE)
        {
            Serial.println("ACK wyslany poprawnie.");
        }
        else
        {
            Serial.print("Blad wysylania ACK: ");
            Serial.println(txState);
        }

        // =================================
        // POWROT DO ODBIORU
        // =================================

        digitalWrite(PIN_RF_SW, HIGH);
        delay(2);

        Serial.println("Powrot do nasluchiwania.");
    }
    else if (state == RADIOLIB_ERR_RX_TIMEOUT)
    {
        // Brak pakietu - ponawiamy nasluchiwanie
    }
    else if (state == RADIOLIB_ERR_CRC_MISMATCH)
    {
        Serial.println("Odebrano pakiet z bledem CRC.");
    }
    else
    {
        Serial.print("Blad odbioru: ");
        Serial.println(state);
        delay(100);
    }
}
