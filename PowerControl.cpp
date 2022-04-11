// Input monitoring and relay control of power to DSP and amps

#include "PowerControl.h"
#include <MiniDSP.h>


void sourceSensing::Init() {

    // Pins
    pinMode(PIN_DIGITAL_TRIGGER, INPUT_PULLUP);
    pinMode(PIN_ANALOG_TRIGGER, INPUT_PULLUP);
    pinMode(PIN_ANALOG_L, INPUT);
    pinMode(PIN_ANALOG_R, INPUT);

    // Use 12-bit resolution
    // 1 LSB = 0.88 mV
    analogReadResolution(12);
    analogReference(AR_DEFAULT); // 3.6V

    // Init signal history
    analogLastL = 0;
    analogLastR = 0;
    lastSignalTime = millis();

    // Do initial signal checks
    signalChecks(); 

};

void sourceSensing::signalChecks() {

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

bool sourceSensing::analogImpedanceTest() {

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

// Power control

void powerControl::Init() {

    pinMode(PIN_DSP_RELAY, OUTPUT);
    pinMode(PIN_AMP_RELAY, OUTPUT);

    ampsOn(false);
    dspOn(false);
}

void powerControl::dspOn(bool on) {

    if (on) {
        digitalWrite(PIN_DSP_RELAY, HIGH);  // Assumes HIGH is on
        dspPowerOn = true;
    }


}

uint8_t powerControl::ampsOn(bool on) {

    if (digitalRead(PIN_DSP_RELAY) && ourMiniDSP.connected()) 
    {
        digitalWrite(PIN_AMP_RELAY, HIGH);   // Assumes HIGH is on
        return 1;
    } else {
        return 0;
    }
}