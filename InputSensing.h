// Input sensing

#pragma once

#include <Arduino.h>
//#include <MiniDSP.h>

#define PIN_DIGITAL_TRIGGER A3
#define PIN_ANALOG_TRIGGER A4
#define PIN_ANALOG_L A1
#define PIN_ANALOG_R A2

#define ANALOG_SIGNAL_THRESHOLD         10      // 8.8 mV
#define ANALOG_SIGNAL_HOLDUP            10000   // 10 seconds
#define ANALOG_IMPEDANCE_THRESHOLD      0.9     // of full scale

//extern MiniDSP ourMiniDSP;      // This may be better as a pointer provided to the constructor.

class InputSensing {
    
    // Constructor
    InputSensing() {
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
