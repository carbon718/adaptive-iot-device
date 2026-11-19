# Połączenia modułów - urządzenie dodatkowe

## Wstęp

Na potrzeby komunikacji LoRa w trybie peer-to-peer zbudowano drugie urządzenie, składające się z płytki NUCLEO-L476RG oraz modułu Wio-SX1262.

Urządzenie odbiera dane przesyłane przez LoRa, sprawdza ich poprawność i odsyła do urządzenia głównego potwierdzenia (ACK).

## Schemat połączeń

### Moduł NUCLEO

Płytka NUCLEO jest zasilana przez port mini-USB. Przez ten sam port dostępny jest wirtualny port szeregowy (ST-LINK VCP), co ułatwia analizę działania i debugowanie.

### Moduł LoRa

Moduł podłączono tak samo jak w [urządzeniu głównym](01-hardware-setup.md), czyli przez SPI1.

| Pin NUCLEO | Pin Wio-SX1262 | Funkcja / opis |
|---|---|---|
| +3V3 | 3V3 | Zasilanie modułu |
| GND | GND | Wspólna masa |
| PA5 | SCK | Zegar SPI |
| PA6 | MISO | Dane SX1262 → STM32 |
| PA7 | MOSI | Dane STM32 → SX1262 |
| PB6 | NSS | Wybór układu (Chip Select) |
| PA9 | RST | Reset modułu |
| PC7 | BUSY | Sygnalizacja zajętości układu |
| PA8 | DIO1 | Przerwanie (IRQ) |
| PB10 | RF_SW | Sterowanie przełącznikiem RF |
