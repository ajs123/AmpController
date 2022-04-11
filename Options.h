// Major options

#pragma once

/** Maximum volume, as a multiple of -0.5 dB. 
 *  This may be used to simply limit the volume. 
 *  But if 0, volume can go all the way to 0 dB, and once there, 
 *  the Vol+ button on the remote won't wake up the display. */
constexpr uint8_t MAXIMUM_VOLUME = 2;

/** Time (ms) after waking up that the display will dim and the
 *  volume indicator will go away. */
constexpr uint16_t DIM_TIME = 5000;   

