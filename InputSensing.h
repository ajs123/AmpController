// Input sensing

#pragma once

#include <Arduino.h>
#include "Configuration.h"
#include <math.h>

// Minimum voltage on a trigger input
constexpr float triggerVoltage = 3.0;

// DSP input level below this value counts as silence
const float DSPSilence = -70.0;

constexpr int x = 1;
constexpr float analogRef = 3.3; 
constexpr float analogInputRatio = 3.3/(10 + 3.3);
constexpr int analogBits = 12;
constexpr int triggerADC = triggerVoltage * analogInputRatio / analogRef * (1 << analogBits);

struct trigger_t {
    bool analog;
    bool digital;
    bool operator==(trigger_t x) { return analog == x.analog && digital == x.digital; }
    trigger_t operator!() const { return {!analog, !digital}; }
    trigger_t operator&(trigger_t x) const { return {analog && x.analog, digital && x.digital}; }
    operator bool() const { return analog || digital; }
};

// Callbacks
extern void onTriggerRise(trigger_t triggers);
extern void onTriggerFall(trigger_t triggers);
extern void onSilence();

// Can't use the following because exp() isn't constexpr
//constexpr float FIRCoeff(const float tau, const float samplePeriod) { return exp(-tau/samplePeriod); };
//const int dTrigger = FIRCoeff(0.1, 0.050) * 100;

constexpr float dTrigger = 0.135;   // tau = 0.1 Hz, samplePeriod = 50 ms

// Single pole IIR filter
template <class T> class IIR {

    public:
        IIR(T coeff) : _x{0}, _coeff{coeff} {}
        IIR(T coeff, T initX) : _x{initX}, _coeff{coeff} {}

        // @brief takes input and returns the new output
        T next(T u) { return _x += _coeff * (u - _x); };

        // @brief returns the current output
        float output() { return _x; };

    private:
        T _x;
        T _coeff;
};

//template <> int IIR<int>::next(int u) { return _x += (_coeff * (u - _x)) / 100; };

// Single-pole IIR filter using integer arithmetic
class IIIR {
    public:
        IIIR(float coeff) : _x{0}, _coeff{int(coeff * 100)} {}
        IIIR(float coeff, int initX) : _coeff{int(coeff * 100)}, _x{initX} {}

        // @brief takes input and returns the new output
        int next(int u) { return _x += _coeff * (u - _x) / 100; };

       // @brief returns the current output
       float output() { return _x; };
       
    private:
        int _x;
        int _coeff;
};

// Persistence filter. Changes state on high or low for N samples
class SampleTrigger {
    public:
        SampleTrigger(int threshold, int samplesHigh, int samplesLow) : 
            _threshold{threshold}, 
            _samplesHigh{samplesHigh}, _samplesLow{samplesLow} {}

        // @brief takes input and returns the current state
        bool next(int input);

        // @brief gets the current state
        bool get() { return _state; }

    private:
        int _threshold;
        int _samplesHigh;
        int _samplesLow;
        int _howLong {0};
        bool _state {false};
};

// Timed trigger. Goes high on exceeding threshold and stays high for specified time
template <class T> class TimedTrigger {
    public:
        TimedTrigger(T threshold, uint32_t minTime) :
            _threshold {threshold}, _minTime {minTime} {}

        // @brief takes input and returns the current state
        bool next(T input) {
            if (input >= _threshold) {
                _state = true;
                _whenHigh = millis();
            } else {
                _state = ((millis() - _whenHigh) < _minTime);
            }
            return _state;
        }

        // @brief gets the current state
        bool get() { return _state; }

    private:
        T _threshold;
        uint32_t _whenHigh { 0 };
        uint32_t _minTime { 2000 };
        bool _state {false};
};

const IIIR filteredInput0(dTrigger);
const SampleTrigger trigger0(triggerADC, 5, 10);

// Trigger monitor. Samples the inputs through IIR and persistence filters to
// obtain the trigger state and invoke callbacks
class TriggerSensing {

    public:
        TriggerSensing() {};

        // @brief Setup and initialization
        void begin();

        // @breif Update triggers
        trigger_t update();

        // @brief Update triggers and invoke callbacks.
        // Callbacks are passed trigger_t with rise or fall flags for each input
        // @return Current trigger state
        trigger_t task();

        // @brief Current trigger state
        trigger_t getTriggers() { return triggers; };

    private:
        IIIR filteredAnalog {filteredInput0};   // Quiet any bumps in the inputs
        IIIR filteredDigital {filteredInput0};

        SampleTrigger analogTrigger {trigger0}; // React to changes after
        SampleTrigger digitalTrigger {trigger0};

        trigger_t triggers {false, false};
        trigger_t lastTriggers {false, false};
};

constexpr uint32_t oneMinute = 60000; // ms
class InputMonitor {

    public:
        InputMonitor(uint32_t silentTime) : _silentTime{silentTime * oneMinute} {}

        // @brief Reset the silence timer
        void resetTimer() { lastSound = millis(); }

        // @brief Set the silence timout
        void setTimout(uint32_t minutes) { _silentTime = minutes * oneMinute; }

        // @brief Evaluate levels and invoke callback on silence
        // @return True if silent for at least the 
        bool task(float leftLevel, float rightLevel);

    private:
        uint32_t _silentTime;
        uint32_t lastSound{0};
};
