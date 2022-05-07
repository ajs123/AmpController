// Major options

#pragma once
#include <Arduino.h>

/** Maximum volume. 
 *  This may be used to simply limit the volume. 
 *  If 0, volume can go all the way to 0 dB, and once there, 
 *  the Vol+ button on the remote won't wake up the display. */
constexpr uint8_t MAXIMUM_VOLUME = -1.0 / -0.5;

/** Time (ms) after waking up that the display will dim and the
 *  volume indicator will go away. */
constexpr uint16_t DIM_TIME = 5 * 1000;   

/** Minimum level in dB for the animated level display */
#define MINBARLEVEL -90
