#pragma once
#include <Arduino.h>

bool loraInit();
bool loraSendMessage(const uint8_t* frame, size_t length);