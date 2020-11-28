#pragma once




struct __attribute__((packed)) ESPMAN_rtc_data {
    bool performingSetup;
    uint8_t sketchMD5[16];
}; 


