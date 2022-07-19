// Relay control of power to DSP and amps
// Includes input detection

#pragma once

#include <Arduino.h>
#include "USB_Host_Shield/MiniDSP.h"

#define PIN_DSP_RELAY 10
#define PIN_AMP_RELAY 11

extern MiniDSP ourMiniDSP;      // This may be better as a pointer provided to the constructor.

class PowerControl {

    /**
     * @brief Construct a new PowerControl object
     * 
     */
    PowerControl() {
        Init();
    }

    public:

        /** @brief Turn the DSP on or off */
        void dspOn(bool on);

        /** Turn the amps on or off.
         *  Turn on is contingent upon the dsp being connected.
         *  Turn off is unconditional.
         * @param on Turn on (true) or off (false)
         * @return on/off state: 0 = off, 1 = on
        */
        uint8_t ampsOn(bool on);

        /** Turn the amps on or off.
         *  Turn on is contingent upon the dsp being connected.
         *  Turn off is unconditional.
         * @param on Turn on (true) or off (false)
         * @param schedule If the dsp isn't connected, schedule amp turn-on for when it is.
         * @return on/off state: 0 = off, 1 = on, 2 = scheduled
        */
        uint8_t ampsOn(bool on, bool schedule);

    protected:

        //MiniDSP * dsp;
        bool ampsOnScheduled;
        bool dspPowerOn;
        void Init();

};