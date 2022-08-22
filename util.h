// Utility functions
#pragma once

// OLED contrast from brightness
inline uint8_t contrast(uint8_t brightness) {
    int val = (1 << brightness) - 1;
    return (uint8_t)(val & 0xFF);
}

// Constrain between low and high limits
template<class T> 
inline T limit(const T& value, const T& min, const T& max) { 
  T v = (value < min) ? min : value;
  return (v > max) ? max : v; 
  }

// Flotaing point approximately equal
inline bool fEqual(float x, float y, float p = 0.1) {
  return (abs(x - y) < p);
}

