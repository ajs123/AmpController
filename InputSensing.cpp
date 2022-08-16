// Input monitoring

#include <Arduino.h>
#include "InputSensing.h"
//#include <MiniDSP.h>

extern Options & ampOptions;

//template <> int IIR<int>::next(int u) { return _x += (_coeff * (u - _x)) / 100; };

        bool SampleTrigger::next(int input) {
            if (_state) {
                if (input < _threshold) {
                    _howLong++;
                    if (_howLong >= _samplesLow) {
                        _state = false;
                        _howLong = 0;
                    }
                } else _howLong = 0; 
            } else {
                if (input > _threshold) {
                    _howLong++;
                    if (_howLong >= _samplesHigh) {
                        _state = true;
                        _howLong = 0;
                    }
                } else _howLong = 0;
            }
            return _state;
        };

void TriggerSensing::begin() {

    // Pins
    pinMode(ANALOG_TRIGGER_PIN, INPUT);
    pinMode(DIGITAL_TRIGGER_PIN, INPUT);

    // 1 LSB = 0.88 mV
    analogReadResolution(12);
    analogReference(AR_DEFAULT);    // 3.6V
};

trigger_t TriggerSensing::update() {
    lastTriggers = triggers;
    triggers = {analogTrigger.next(filteredAnalog.next(analogRead(ANALOG_TRIGGER_PIN))),
                digitalTrigger.next(filteredDigital.next(analogRead(DIGITAL_TRIGGER_PIN)))};
    return triggers;
}

trigger_t TriggerSensing::task() {
    trigger_t rose{false, false};
    trigger_t fell{false, false};
    update();
    if (lastTriggers == triggers) return triggers;
    rose = triggers & !lastTriggers;
    fell = !triggers & lastTriggers;
    if (rose) onTriggerRise(rose);
    if (fell) onTriggerFall(fell);
    lastTriggers = triggers;
    return triggers;
}

bool InputMonitor::task(float leftLevel, float rightLevel) {
    if (_silentTime < oneMinute) return false; 
    uint32_t time = millis();
    bool ret = false;
    //if (max(rightLevel, leftLevel) < DSPSilence) {
    if (max(rightLevel, leftLevel) < (ampOptions.silence * -0.5)) {
        uint32_t howLong = time - lastSound;
        if (howLong > _silentTime) {
            onSilence();
            lastSound = time;
            ret = true;
        }
    } else {
        lastSound = time;
    }
    return ret;
}