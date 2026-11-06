#pragma once

bool loraInit();

bool loraSendMessage(const uint8_t* frame, size_t length);