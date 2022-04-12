// Input monitoring

#include <Arduino.h>
#include "InputSensing.h"
#include <MiniDSP.h>

void InputSensing::Init() {

    // Pins
    pinMode(PIN_DIGITAL_TRIGGER, INPUT_PULLUP);  // Open collector
    pinMode(PIN_ANALOG_TRIGGER, INPUT_PULLUP);
    pinMode(PIN_ANALOG_L, INPUT);
    pinMode(PIN_ANALOG_R, INPUT);

    // Use 12-bit resolution, 3.6V reference
    // 1 LSB = 0.88 mV
    analogReadResolution(12);
    analogReference(AR_DEFAULT);

    // Init signal history
    analogLastL = 0;
    analogLastR = 0;
    lastSignalTime = millis();

    // Do initial signal checks
    signalChecks(); 

};

void InputSensing::signalChecks() {

    // Triggers
    digitalTriggerPresent = !digitalRead(PIN_DIGITAL_TRIGGER);
    analogTriggerPresent = !digitalRead(PIN_ANALOG_TRIGGER);
    
    // Analog levels
    // The simplest approach is that anything over threshold counts
    // This is not robust against little spikes, etc.

    bool analogDetected = false;

    uint16_t aSignal = max(analogRead(PIN_ANALOG_L), analogRead(PIN_ANALOG_R));
    if (aSignal >= ANALOG_SIGNAL_THRESHOLD) {
        lastSignalTime = millis();
        analogDetected = true;
        analogSignalPresent = true;
    } else {
        analogSignalPresent = !( (millis() - lastSignalTime) > ANALOG_SIGNAL_HOLDUP );
    }

    if (analogDetected) {
        analogImpedanceLow = true;
    } else {
        analogImpedanceLow = analogImpedanceTest();
    }

}

bool InputSensing::analogImpedanceTest() {

    // If an input is disconnected, or connected to a high-impedance (turned off)
    // source, then turning on the input pullup should bring the pin voltage very close
    // to Vdd. Otherwise, it must be connected to a source.
    // 
    // This will read incorrectly if the signal came on right before this was called,
    // but that will get corrected on the next update.

    uint16_t baseline;
    uint16_t purturbed;
    constexpr uint16_t thresh = (uint16_t) (4096 * 3.3 / 3.6 * ANALOG_IMPEDANCE_THRESHOLD);

    pinMode(PIN_ANALOG_L, INPUT_PULLUP);
    pinMode(PIN_ANALOG_R, INPUT_PULLUP);
    delay(10);
    bool high = (analogRead(PIN_ANALOG_L) > thresh) && (analogRead(PIN_ANALOG_R) > thresh);
    pinMode(PIN_ANALOG_L, INPUT);
    pinMode(PIN_ANALOG_R, INPUT);

    return !high;

}
