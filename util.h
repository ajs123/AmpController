// Utility functions
#pragma once

// OLED contrast from brightness
inline uint8_t contrast(uint8_t brightness) {
    int val = (1 << brightness) - 1;
    return (uint8_t)(val & 0xFF);
}