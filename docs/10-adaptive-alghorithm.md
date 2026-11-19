# Algorytm adaptacyjnego wyboru łącza

## Założenia

- **LoRa** jest łączem preferowanym, ponieważ zużywa mniej energii. Jej wady to ograniczony payload (do 242 B), duty cycle 1% oraz długi czas dostarczenia.
- **LTE-M** jest łączem alternatywnym - pozwala przesłać payload do 1447 B, ale zużywa więcej energii.
- Każda ramka ma określony maksymalny dopuszczalny czas dostarczenia `maxDelayMs`.

## Dane wejściowe

| Dane | Źródło |
|---|---|
| `payloadLength`, `maxDelayMs` | ramka do wysłania |
| `loraReady` | `true`, jeśli inicjalizacja SX1262 się powiodła |
| `loraBlockedUntil` | koniec blokady duty cycle, wyznaczany po każdej wysyłce LoRa jako 99 × ToA (czas nadawania) |
| `loraRssi`, `loraSnr`, `loraAckTime` | ostatni poprawny ACK od Peer B |
| `loraFailCount` | liczba kolejnych wysyłek LoRa bez ACK |
| `lteReady` | moduł zarejestrowany w sieci (`AT+CEREG?` zwraca stan 1 lub 5) |
| `lteRsrp`, `lteSinr` | aktualne wartości z `AT+CPSI?` |

## Progi

| Parametr | Wartość | Uzasadnienie |
|---|---|---|
| `LORA_RSSI_MIN` | -120 dBm | blisko czułości odbiornika dla SF9 |
| `LORA_SNR_MIN` | -7,5 dB | 5 dB zapasu nad limitem demodulacji SF9 (-12,5 dB) |
| `LORA_ACK_MAX_AGE` | 10 min | starszy pomiar uznawany jest za nieaktualny |
| `LORA_MAX_FAILS` | 3 | po 3 kolejnych nieudanych próbach LoRa uznawana jest za niedostępną |
| `LTE_RSRP_MIN` | -120 dBm | minimalny akceptowalny poziom sygnału LTE-M |
| `LORA_MAX_DELIVERY` | `ToA + ACK_TIMEOUT_MS` | maksymalny czas dostarczenia przez LoRa |

## Algorytm

### Krok 1. Dostępność LoRa

```
loraAvailable = loraReady
                AND now >= loraBlockedUntil          // nie trwa blokada duty cycle
                AND loraFailCount < LORA_MAX_FAILS
                AND payloadLength <= 242
```

### Krok 2. Dostępność LTE-M

W obecnej konfiguracji moduł SIM7080G nie korzysta z trybów oszczędzania energii, więc sprawdzenie poniższego warunku nie zwiększa zużycia energii. Gdyby używany był tryb PSM (lub inny tryb oszczędny), odczyt wymagałby wybudzenia modułu. Wtedy wartość `lteRsrp` należałoby odczytywać okresowo i zapamiętywać, np. podczas ostatniej transmisji.

```
lteAvailable = lteReady
               AND lteRsrp >= LTE_RSRP_MIN
```

### Krok 3. Ocena jakości LoRa (na podstawie ostatniego ACK)

```
IF brak ACK OR (now - loraAckTime > LORA_ACK_MAX_AGE):
    loraGood = true        // brak aktualnych danych - LoRa jest preferowana, więc zakładamy optymistycznie, że działa

ELSE:                      // na podstawie ostatniego ACK
    loraGood = (loraRssi >= LORA_RSSI_MIN) AND (loraSnr >= LORA_SNR_MIN)
```

### Krok 4. Wybór łącza

```
IF NOT loraAvailable AND NOT lteAvailable:
    → brak łącza: ramka czeka na następny cykl

ELSE IF loraAvailable AND loraGood AND maxDelayMs >= LORA_MAX_DELIVERY:
    → LoRa                 // łącze preferowane, spełnia wymaganie czasowe

ELSE IF lteAvailable:
    → LTE-M                // fallback na LTE-M

ELSE:
    → LoRa                 // ostateczność: LoRa mimo parametrów poniżej progów
                           // (jest szansa, że dane mimo to zostaną dostarczone)
```

### Krok 5. Wysyłka i aktualizacja stanu

```
IF wybrano LoRa:
    wyślij ramkę (1 próba)
    loraBlockedUntil = now + ToA × 99          // duty cycle 1%: ToA / (ToA + przerwa) = 1%
    IF ACK:    loraFailCount = 0; zapisz loraRssi, loraSnr, loraAckTime
    ELSE:      loraFailCount++
               IF lteAvailable: → wyślij przez LTE-M (fallback)

IF wybrano LTE-M:
    wyślij ramkę (do LTE_MAX_ATTEMPTS = 3 prób)
    IF brak ACK AND loraAvailable: → próba przez LoRa (fallback)
```

### Krok 6. Odzyskiwanie LoRa

Gdy LoRa zostanie wyłączona po przekroczeniu `LORA_MAX_FAILS`, co N cykli (w implementacji `LORA_PROBE_EVERY = 10`) wysyłana jest przez nią ramka próbna. Pierwszy otrzymany ACK zeruje `loraFailCount` i LoRa wraca do użycia.

## Test

Parametry testu:

- rozmiar ramki: `TEST_FRAME_SIZE = 64 B`,
- interwał wysyłki: `MESSAGE_INTERVAL_MS = 15000` - celowo krótszy niż blokada duty cycle (ok. 39 s dla ToA ≈ 390 ms), aby wymusić przełączanie na LTE-M,
- co 5. ramka jest pilna (`maxDelayMs = 2000 ms`, czyli mniej niż czas dostarczenia przez LoRa, ok. 3,4 s).

Wyniki:

- LoRa wybierana jest co 3. cykl (SEQ 1, 4, 7, 11, 14, 17) - po każdej wysyłce jest blokowana przez ok. 39 s, więc w dwóch kolejnych cyklach ramki trafiają na LTE-M.
- Pilna ramka SEQ=10 trafiła na LTE-M, mimo że LoRa była dostępna - LoRa nie spełniała wymagania czasowego.
- Wszystkie ramki zostały dostarczone przy pierwszej próbie. Czas aktywności wynosił ok. 650 ms dla LoRa i ok. 380 ms dla LTE-M (ACK po ok. 320 ms).

```
 > SX1262 initialized.
 > SIM7080G initialized, UDP socket open.
 > LoRa ready: 1, LTE-M ready: 1
 > SEQ=1 maxDelay=60000 ms | LoRa: avail=1 blocked=0 s fails=0 good=1 RSSI=0 SNR=0 delivery=3390 ms | LTE-M: avail=1 RSRP=-108 SINR=6 | -> LoRa
 > Attempt 1/1, SEQ=1
 > TX (64 B): AA 55 00 01 01 00 00 00 01 00 33 72 8A B2 67 F8 79 7A 38 3B 5C 37 BD FD D2 32 D8 F5 35 A7 7B C3 24 DE D5 F7 71 EF 98 FB A3 D9 A9 48 F6 12 21 98 A1 59 67 FC 84 37 34 25 83 32 3C 06 F3 BF 02 B8 
 > RX (17 B): AA 55 00 01 02 00 00 00 01 00 04 00 01 DE 0A 89 A0 
 > Delivered via LoRa.
 > Active: 651 ms, deep sleep: 14349 ms
 > ----------------------------------------
 > SEQ=2 maxDelay=60000 ms | LoRa: avail=0 blocked=24 s fails=0 good=1 RSSI=-34 SNR=10 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=6 | -> LTE-M
 > LTE attempt 1/3, SEQ=2
 > TX (64 B): AA 55 00 01 01 00 00 00 02 00 33 A4 46 B0 41 4D A7 77 EA 0A CD 71 6C 73 12 A1 2F B4 84 65 BF 4B F5 C6 4F B6 99 03 F6 AF 0E 03 C8 20 48 4C 9F 74 E8 FF 2F 0F 34 A6 92 0F 79 24 93 1E 8C 88 46 0B 
 > RX (17 B): AA 55 00 01 02 00 00 00 02 00 04 00 02 80 80 4E DA 
 > ACK after 319 ms
 > Delivered via LTE-M.
 > Active: 379 ms, deep sleep: 14621 ms
 > ----------------------------------------
 > SEQ=3 maxDelay=60000 ms | LoRa: avail=0 blocked=9 s fails=0 good=1 RSSI=-34 SNR=10 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=4 | -> LTE-M
 > LTE attempt 1/3, SEQ=3
 > TX (64 B): AA 55 00 01 01 00 00 00 03 00 33 66 7C 25 F7 F7 6A E6 5F 33 89 7E C6 80 38 7B EF B2 5D AE C2 E4 EB 1B 01 A2 E1 69 5A 3C D0 10 24 EB 5F D7 5A 3F FF CF E3 F7 7E CE 24 90 45 17 A3 3C 77 5B 05 E5 
 > RX (17 B): AA 55 00 01 02 00 00 00 03 00 04 00 02 80 80 8E CA 
 > ACK after 320 ms
 > Delivered via LTE-M.
 > Active: 378 ms, deep sleep: 14622 ms
 > ----------------------------------------
 > SEQ=4 maxDelay=60000 ms | LoRa: avail=1 blocked=0 s fails=0 good=1 RSSI=-34 SNR=10 delivery=3390 ms | LTE-M: avail=1 RSRP=-106 SINR=6 | -> LoRa
 > Attempt 1/1, SEQ=4
 > TX (64 B): AA 55 00 01 01 00 00 00 04 00 33 0E 9F E8 F1 13 D8 18 58 AD 4C AC A8 DA DA 06 A6 A9 40 6A AB 02 83 42 42 5D C1 2E FF 2E 9A EF 45 51 5D 96 95 AE AC D2 88 64 C1 FE CA 02 CB 0E 86 29 99 A8 0D B5 
 > RX (17 B): AA 55 00 01 02 00 00 00 04 00 04 00 01 E2 0B 49 25 
 > Delivered via LoRa.
 > Active: 652 ms, deep sleep: 14348 ms
 > ----------------------------------------
 > SEQ=5 maxDelay=2000 ms | LoRa: avail=0 blocked=24 s fails=0 good=1 RSSI=-30 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-106 SINR=4 | -> LTE-M
 > LTE attempt 1/3, SEQ=5
 > TX (64 B): AA 55 00 01 01 00 00 00 05 00 33 D6 48 1D 22 AF D2 34 E8 E3 C1 61 0B 3F A9 77 0B 33 55 29 33 51 F1 2E 81 6E D4 71 7F C8 B5 DB 75 0A 4A E8 A3 41 D9 99 F0 6D A7 EE 89 1C 71 99 1D 32 A7 3F 44 D6 
 > RX (17 B): AA 55 00 01 02 00 00 00 05 00 04 00 02 80 80 8E AC 
 > ACK after 320 ms
 > Delivered via LTE-M.
 > Active: 380 ms, deep sleep: 14620 ms
 > ----------------------------------------
 > SEQ=6 maxDelay=60000 ms | LoRa: avail=0 blocked=9 s fails=0 good=1 RSSI=-30 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=2 | -> LTE-M
 > LTE attempt 1/3, SEQ=6
 > TX (64 B): AA 55 00 01 01 00 00 00 06 00 33 88 66 3F 6B 56 B7 9A B9 A7 24 CD 0B 11 D1 8D B8 21 41 85 62 CC 61 F6 FE FE 36 D0 B9 A2 15 CF 1D 65 1A 00 50 07 93 DC D8 F7 B7 A1 0C F7 E3 7C EB 6D DD 7A 8A 52 
 > RX (17 B): AA 55 00 01 02 00 00 00 06 00 04 00 02 80 80 8E 9F 
 > ACK after 320 ms
 > Delivered via LTE-M.
 > Active: 379 ms, deep sleep: 14621 ms
 > ----------------------------------------
 > SEQ=7 maxDelay=60000 ms | LoRa: avail=1 blocked=0 s fails=0 good=1 RSSI=-30 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=6 | -> LoRa
 > Attempt 1/1, SEQ=7
 > TX (64 B): AA 55 00 01 01 00 00 00 07 00 33 F8 8A C6 35 45 34 F3 46 0B AE 64 EF 86 BF A4 FA 6A 84 6B 90 64 E7 13 25 20 A3 8E 50 3E 6C EA B7 F7 3B 0A 11 01 84 4D 3B F6 D1 8E 94 25 10 6B 57 AE 1F BE 67 95 
 > RX (17 B): AA 55 00 01 02 00 00 00 07 00 04 00 01 DA 0B 89 05 
 > Delivered via LoRa.
 > Active: 648 ms, deep sleep: 14352 ms
 > ----------------------------------------
 > SEQ=8 maxDelay=60000 ms | LoRa: avail=0 blocked=24 s fails=0 good=1 RSSI=-38 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-106 SINR=6 | -> LTE-M
 > LTE attempt 1/3, SEQ=8
 > TX (64 B): AA 55 00 01 01 00 00 00 08 00 33 46 6B CD 3B 19 55 4A 20 00 A3 B0 AF 63 60 72 2A 09 95 3A 58 8D 10 55 D5 1E E5 6E 46 F3 34 8C D0 55 87 BF B2 1A 01 9A 46 32 13 1D A8 A0 CB C3 D9 1B 19 EE 41 DF 
 > RX (17 B): AA 55 00 01 02 00 00 00 08 00 04 00 02 80 80 4E 70 
 > ACK after 322 ms
 > Delivered via LTE-M.
 > Active: 380 ms, deep sleep: 14620 ms
 > ----------------------------------------
 > SEQ=9 maxDelay=60000 ms | LoRa: avail=0 blocked=9 s fails=0 good=1 RSSI=-38 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-106 SINR=4 | -> LTE-M
 > LTE attempt 1/3, SEQ=9
 > TX (64 B): AA 55 00 01 01 00 00 00 09 00 33 C4 1C 0C 7B 2F 2F 49 AF C7 A7 E8 DF 49 0E F8 72 7B B0 34 77 78 22 6C 81 4A 54 A7 40 A0 74 F7 E7 69 5A B5 BB 2D 1C BC 83 7A 9A 44 88 E1 E6 B2 12 22 5C 31 B0 C5 
 > RX (17 B): AA 55 00 01 02 00 00 00 09 00 04 00 02 80 80 8E 60 
 > ACK after 323 ms
 > Delivered via LTE-M.
 > Active: 382 ms, deep sleep: 14618 ms
 > ----------------------------------------
 > SEQ=10 maxDelay=2000 ms | LoRa: avail=1 blocked=0 s fails=0 good=1 RSSI=-38 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=4 | -> LTE-M
 > LTE attempt 1/3, SEQ=10
 > TX (64 B): AA 55 00 01 01 00 00 00 0A 00 33 BC 5C 98 3F 38 CF F9 67 F7 0C 1A 78 72 C7 D9 E4 00 E3 5A EC 96 34 8E 03 53 00 A1 7E 29 BE A4 E4 F5 2B D0 AC C2 F5 6C BB 73 CD A0 63 48 91 D8 72 E8 CC E9 17 03 
 > RX (17 B): AA 55 00 01 02 00 00 00 0A 00 04 00 02 80 80 8E 53 
 > ACK after 316 ms
 > Delivered via LTE-M.
 > Active: 376 ms, deep sleep: 14624 ms
 > ----------------------------------------
 > SEQ=11 maxDelay=60000 ms | LoRa: avail=1 blocked=0 s fails=0 good=1 RSSI=-38 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=4 | -> LoRa
 > Attempt 1/1, SEQ=11
 > TX (64 B): AA 55 00 01 01 00 00 00 0B 00 33 20 A8 52 2D D8 66 6B 8A 04 74 FE 23 53 1E 1D 6C 35 6A CA BA C8 34 E3 CF 8E E5 31 D2 34 00 B1 C6 89 B3 03 59 28 3D E4 94 EC C7 29 97 1C 66 C5 5E 94 DE 6D AF 2B 
 > RX (17 B): AA 55 00 01 02 00 00 00 0B 00 04 00 01 DA 0B 89 C9 
 > Delivered via LoRa.
 > Active: 647 ms, deep sleep: 14353 ms
 > ----------------------------------------
 > SEQ=12 maxDelay=60000 ms | LoRa: avail=0 blocked=24 s fails=0 good=1 RSSI=-38 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=-2 | -> LTE-M
 > LTE attempt 1/3, SEQ=12
 > TX (64 B): AA 55 00 01 01 00 00 00 0C 00 33 ED 5C 32 BD 56 74 EC 4C 0B EC 02 8F 57 A0 00 89 79 B6 AB D1 95 EC E9 25 EE E5 E4 82 DA 91 CD DA DB 23 58 63 77 CE 81 4C 20 41 16 7B D8 73 4E 87 C7 65 D4 F9 C5 
 > RX (17 B): AA 55 00 01 02 00 00 00 0C 00 04 00 02 80 80 8E 35 
 > ACK after 322 ms
 > Delivered via LTE-M.
 > Active: 377 ms, deep sleep: 14623 ms
 > ----------------------------------------
 > SEQ=13 maxDelay=60000 ms | LoRa: avail=0 blocked=9 s fails=0 good=1 RSSI=-38 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=4 | -> LTE-M
 > LTE attempt 1/3, SEQ=13
 > TX (64 B): AA 55 00 01 01 00 00 00 0D 00 33 E5 69 E3 73 7F 56 D2 5C DB 42 DA 88 A9 F6 E0 06 9C B8 24 A6 E2 90 B4 0C 8C CB 04 E9 40 9D E6 30 C4 28 EB AB 70 46 01 FC EE 14 D6 9F D9 15 9F 8A 12 8B 69 C0 8C 
 > RX (17 B): AA 55 00 01 02 00 00 00 0D 00 04 00 02 80 80 4E 25 
 > ACK after 318 ms
 > Delivered via LTE-M.
 > Active: 378 ms, deep sleep: 14622 ms
 > ----------------------------------------
 > SEQ=14 maxDelay=60000 ms | LoRa: avail=1 blocked=0 s fails=0 good=1 RSSI=-38 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-106 SINR=4 | -> LoRa
 > Attempt 1/1, SEQ=14
 > TX (64 B): AA 55 00 01 01 00 00 00 0E 00 33 9E 90 50 99 9C BC 66 97 CA DE C5 70 85 A9 4E 72 65 E2 92 F7 18 CF D8 7E 7F 93 47 F1 A1 7A 03 8D 47 2C 58 09 A6 D4 FF 0D B7 BD C6 75 E6 0C ED 51 E2 79 77 EB 02 
 > RX (17 B): AA 55 00 01 02 00 00 00 0E 00 04 00 01 E0 0B 29 8E 
 > Delivered via LoRa.
 > Active: 648 ms, deep sleep: 14352 ms
 > ----------------------------------------
 > SEQ=15 maxDelay=2000 ms | LoRa: avail=0 blocked=24 s fails=0 good=1 RSSI=-32 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=2 | -> LTE-M
 > LTE attempt 1/3, SEQ=15
 > TX (64 B): AA 55 00 01 01 00 00 00 0F 00 33 1F E7 F1 0E 32 0E F5 9E 1A F5 B8 7E 14 79 66 2B DB 6A 4C 7B 39 60 AA 31 5E B3 48 DB DD A8 95 A8 38 45 8C 1D 8B 4B CF FE 4E 99 08 53 AC 65 50 6E 97 CF 35 CD 99 
 > RX (17 B): AA 55 00 01 02 00 00 00 0F 00 04 00 02 80 80 8E 06 
 > ACK after 323 ms
 > Delivered via LTE-M.
 > Active: 378 ms, deep sleep: 14622 ms
 > ----------------------------------------
 > SEQ=16 maxDelay=60000 ms | LoRa: avail=0 blocked=9 s fails=0 good=1 RSSI=-32 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-106 SINR=2 | -> LTE-M
 > LTE attempt 1/3, SEQ=16
 > TX (64 B): AA 55 00 01 01 00 00 00 10 00 33 BF 34 7C 2F F1 37 93 42 DC 02 68 5A 5B D3 6F 02 3F 39 EC 74 ED E4 89 22 81 C2 F0 E4 6F 6F 96 1C 18 B7 E8 D5 98 6D 77 0D D6 55 D7 B7 5A C1 C0 47 73 D2 FD B4 16 
 > RX (17 B): AA 55 00 01 02 00 00 00 10 00 04 00 02 80 80 4F E8 
 > ACK after 319 ms
 > Delivered via LTE-M.
 > Active: 379 ms, deep sleep: 14621 ms
 > ----------------------------------------
 > SEQ=17 maxDelay=60000 ms | LoRa: avail=1 blocked=0 s fails=0 good=1 RSSI=-32 SNR=11 delivery=3390 ms | LTE-M: avail=1 RSRP=-107 SINR=4 | -> LoRa
 > Attempt 1/1, SEQ=17
 > TX (64 B): AA 55 00 01 01 00 00 00 11 00 33 76 5D 8B 29 5D 5E 1C 15 85 B6 34 A3 F4 AE 6C C3 6C AC 81 93 91 F8 2F 8C 69 0F 8F 6F 45 65 AB 7A 73 7B 84 BF DD 40 69 40 8E 93 4C 33 F8 EF 48 8F 4F F7 BE ED 4E 
 > RX (17 B): AA 55 00 01 02 00 00 00 11 00 04 00 01 DF 0B 18 71 
 > Delivered via LoRa.
 > Active: 648 ms, deep sleep: 14352 ms
 > ----------------------------------------
```
