# Struktura ramki komunikacyjnej

Aby dane były przesyłane w jednolity sposób zarówno przez LTE-M, jak i LoRa, zaproponowano wspólną strukturę ramki (Communication Frame).

Wszystkie pola wielobajtowe zapisywane są w kolejności Big Endian.

## Communication Frame

### Budowa ramki

| HEADER | PAYLOAD | CRC |
|---|---|---|
| 11 B | N B | 2 B |

### Opis pól

| Pole | Rozmiar | Opis |
|---|---|---|
| MAGIC | 2 B | Identyfikator formatu ramki (0xAA55) |
| DEVICE_ID | 2 B | Identyfikator urządzenia |
| FRAME_TYPE | 1 B | 0x01 = DATA, 0x02 = ACK |
| SEQUENCE | 4 B | Numer sekwencyjny ramki |
| PAYLOAD_LENGTH | 2 B | Długość payloadu (N) |
| PAYLOAD | N B | Właściwe dane |
| CRC-16 | 2 B | Suma kontrolna |

## ACK

ACK korzysta z tej samej struktury Communication Frame. Oprócz potwierdzenia odbioru przesyła również parametry łącza radiowego (dla LoRa).

### Budowa ramki ACK

| HEADER | ACK PAYLOAD | CRC |
|---|---|---|
| 11 B | 4 B | 2 B |

### Payload ACK

| Pole | Rozmiar | Opis |
|---|---|---|
| STATUS | 1 B | 0x00 = OK, 0x01 = błąd CRC, 0x02 = inny błąd |
| INTERFACE | 1 B | Interfejs, którym odebrano ramkę: 0x01 = LoRa, 0x02 = LTE-M |
| SIGNAL_POWER | 1 B | RSSI ostatniego pakietu w dBm (int8); dla LTE-M 0x80 |
| SIGNAL_QUALITY | 1 B | SNR ostatniego pakietu w dB (int8); dla LTE-M 0x80 |

## Suma kontrolna CRC-16/MODBUS

Suma kontrolna obliczana jest z pól HEADER i PAYLOAD według następujących parametrów:

- Algorytm: CRC-16/MODBUS
- Wielomian: 0x8005 (odwrócony: 0xA001)
- Wartość początkowa (Init): 0xFFFF
- RefIn / RefOut: true / true
- XorOut: 0x0000
- Zakres: wszystkie bajty HEADER + PAYLOAD (bajty od 0 do 10 + N)

## Ograniczenia rozmiaru danych

Narzut ramki (HEADER + CRC) wynosi 13 B, stąd:

- SIM7080G pozwala przesłać maksymalnie 1460 B, czyli payload do 1447 B,
- SX1262 pozwala przesłać maksymalnie 255 B, czyli payload do 242 B.
