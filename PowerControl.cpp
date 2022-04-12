// Relay control of power to DSP and amps

#include "PowerControl.h"
#include <MiniDSP.h>

void PowerControl::Init() {

    pinMode(PIN_DSP_RELAY, OUTPUT);
    pinMode(PIN_AMP_RELAY, OUTPUT);

    ampsOn(false);
    dspOn(false);
}

void PowerControl::dspOn(bool on) {

    if (on) {
        digitalWrite(PIN_DSP_RELAY, HIGH);  // Assumes HIGH is on
        dspPowerOn = true;
    } else {
        digitalWrite(PIN_DSP_RELAY, LOW);
    }
}

uint8_t PowerControl::ampsOn(bool on) {

    if (digitalRead(PIN_DSP_RELAY) && ourMiniDSP.connected()) // connected() should be sufficient
    {
        digitalWrite(PIN_AMP_RELAY, HIGH);   // Assumes HIGH is on
        return 1;
    } else {
        digitalWrite(PIN_AMP_RELAY, LOW);
        return 0;
    }
}