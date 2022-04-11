// Relay control of power to DSP and amps
// Includes input detection

#pragma once

#include <Arduino.h>
#include <MiniDSP.h>

#define PIN_DIGITAL_TRIGGER A3
#define PIN_ANALOG_TRIGGER A4
#define PIN_ANALOG_L A1
#define PIN_ANALOG_R A2

#define PIN_DSP_RELAY 10
#define PIN_AMP_RELAY 11

#define ANALOG_SIGNAL_THRESHOLD         10      // 8.8 mV
#define ANALOG_SIGNAL_HOLDUP            10000   // 10 seconds
#define ANALOG_IMPEDANCE_THRESHOLD      0.9     // of full scale

extern MiniDSP ourMiniDSP;      // This may be better as a pointer provided to the constructor.

class sourceSensing {
    
    // Constructor
    sourceSensing() {
        Init();
    };

    public:

        // @brief True if the digital trigger is present
        bool digitalTrigger() {return digitalTriggerPresent;};

        // @brief True if the analog trigger is present
        bool analogTrigger() {return analogTriggerPresent;};

        // @brief True if the analog impedance measures low
        bool analogImpedance() {return analogImpedanceLow;};

        // @brief True if there has been recent analog signal activity
        bool analogSignal() {return analogSignalPresent;};

        // Does all checks 
        void signalChecks();

    protected:

        bool digitalTriggerPresent;
        bool analogTriggerPresent;
        bool analogImpedanceLow;
        bool analogSignalPresent;  // True if a signal is, or recently was, present

        // @brief True if the analog source impedance appears to be low
        bool analogImpedanceTest();

        void Init();

    private:

        uint16_t analogLastL;
        uint16_t analogLastR;
        uint32_t lastSignalTime;
};

class powerControl {

    // Constructor
    powerControl() {
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

        MiniDSP * dsp;
        bool ampsOnScheduled;
        bool dspPowerOn;
        void Init();

};