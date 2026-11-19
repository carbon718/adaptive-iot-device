# Połączenia modułów - urządzenie główne

## Wstęp

Urządzenie składa się z następujących modułów:

### NUCLEO

- [NUCLEO-L476RG](https://www.st.com/en/evaluation-tools/nucleo-l476rg.html) – płytka ewaluacyjna z mikrokontrolerem STM32L476RG
- [Dokumentacja (UM1724)](https://download.kamami.pl/p559277-um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)

### Moduł LoRa

- [Wio-SX1262](https://wiki.seeedstudio.com/wio_sx1262/) – moduł LoRa 868 MHz z układem SX1262
- [Dokumentacja SX1262](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)

### Moduł LTE-M

- [M5Stamp CAT-M](https://docs.m5stack.com/en/stamp/Stamp_CatM) – moduł IoT CAT-M / NB-IoT oparty na układzie SIM7080G
- [Dokumentacja SIM7080G](https://www.simcom.com/product/SIM7080G.html)

## Schemat połączeń

### Moduł NUCLEO

![Pinout NUCLEO-L476RG](https://download.kamami.pl/p559277-NUCLEO-L476RG-pinout.png)

Płytka NUCLEO jest zasilana z zewnętrznego zasilacza 5 V. Zgodnie z dokumentacją zworkę JP5 ustawiono w pozycji 2-3 (zasilanie z pinu E5V).

| Pin NUCLEO | Funkcja / opis |
|---|---|
| E5V | Zasilanie z zewnętrznego zasilacza 5 V |
| GND | Masa zasilacza, wspólna masa układu |

### Moduł LoRa

![Pinout Wio-SX1262](https://download.kamami.pl/p1201747-113010003-hardware-1.png)

Moduł komunikuje się z mikrokontrolerem przez SPI1.

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

### Moduł LTE-M

![M5Stamp CAT-M](https://shop.m5stack.com/cdn/shop/products/10_4dd96007-65f7-4d61-a143-5823d2b18393_1200x1200.jpg)

Moduł komunikuje się z mikrokontrolerem przez UART4. Zasilanie 5 V doprowadzono bezpośrednio z zasilacza, równolegle do pinu E5V płytki NUCLEO.

| Pin NUCLEO | Pin M5Stamp CAT-M | Funkcja / opis |
|---|---|---|
| - | 5V | Zasilanie modułu |
| GND | GND | Wspólna masa |
| PA1 | TX | Dane M5Stamp CAT-M → STM32 (UART4_RX) |
| PA0 | RX | Dane STM32 → M5Stamp CAT-M (UART4_TX) |
