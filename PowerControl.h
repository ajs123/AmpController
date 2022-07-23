// Relay control of power to DSP and amps
// Includes input detection

#pragma once

#include <Arduino.h>
#include "Configuration.h"

class PowerControl {

    public:
        PowerControl();

        void begin();

        /**
         * @brief Power on to DSP and amps, disabling the amps first if necessary
         */
        void powerOn();

        /**
         * @brief Power off the DSP and amps. As necessary, disables the amps first,
         * and waits for the disable to take effect.
         */
        void powerOff();

        /**
         * @brief Enable the amps. As necessary, waits for powerup to finish first.
         */
        void ampEnable();

        /**
         * @brief Disable the amps.
         */
        void ampDisable();

    private:
        static const uint32_t powerOnDelay {1000};     // ms from line power to amps fully powered
        static const uint32_t enableDelay {100};       // ms from pulling EN down to amps quiet

        uint32_t whenPowerOn;
        uint32_t whenDisabled;
};