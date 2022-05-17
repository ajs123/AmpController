// Major options

#pragma once
#include <Arduino.h>

/** Maximum volume. 
*/
constexpr uint8_t MAXIMUM_VOLUME = 0;
//constexpr uint8_t MAXIMUM_VOLUME = -1.0 / -0.5;

/** Time (ms) after waking up that the display will dim and the
 *  volume indicator will go away. */
constexpr uint16_t DIM_TIME = 5 * 1000;   

/** Minimum level in dB for the VU meter */
#define MINBARLEVEL -60
