
#include <Arduino.h>
#include <STM32RTC.h>
#include <STM32LowPower.h>

//static const uint32_t SLEEP_MS = 5000;
static const uint32_t SLEEP_MS = 8*8*9*1000;

#ifndef BUILD_EPOCH
#define BUILD_EPOCH 1788220800UL  // 2026-09-01 00:00:00 UTC
#endif

STM32RTC& rtc = STM32RTC::getInstance();

void setup() {
    Serial.begin(115200);
    Serial.println("STM32 RTC + Deep Sleep Example");
    delay(10000);

    pinMode(D13, OUTPUT);
    digitalWrite(D13, LOW);

    // Configure the RTC to use the 32.768 kHz external oscillator (LSE).
    // The RTC continues to keep time while the microcontroller is in sleep mode.
    rtc.setClockSource(STM32RTC::LSE_CLOCK);
    rtc.begin();

    // Set the RTC time only on the first startup.
    // The RTC retains its time across resets as long as backup power is available.
    if (!rtc.isTimeSet()) {
        rtc.setEpoch(BUILD_EPOCH);
    }

   LowPower.begin();
}

void loop() {
    uint32_t subSec = 0;
    uint32_t epoch = rtc.getEpoch(&subSec);

    // Example output:
    // [2026-11-19 18:59:03.123] epoch=1795625943 CMD=PING

    char buf[80];

    snprintf(buf, sizeof(buf),
             "[20%02u-%02u-%02u %02u:%02u:%02u.%03lu] epoch=%lu CMD=%s",
             rtc.getYear(), rtc.getMonth(), rtc.getDay(),
             rtc.getHours(), rtc.getMinutes(), rtc.getSeconds(),
             (unsigned long)subSec, (unsigned long)epoch, "TEST");

    Serial.println(buf);
    delay(1000);

    // Wait until all data has been transmitted over UART before entering sleep mode.
    Serial.flush();

    // Enter STOP mode and wake up after SLEEP_MS using the RTC alarm.
    // Program execution continues from this point after waking up (without a reset).
    LowPower.deepSleep(SLEEP_MS);
}
