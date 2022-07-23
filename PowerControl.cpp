// Relay control of power to DSP and amps

#include "PowerControl.h"

// Hardware-level functions
bool powerIsOn()    { return digitalRead(RELAY_PIN) == HIGH; }
bool ampsEnabled()  { return digitalRead(AMP_ENABLE_PIN) == LOW; }  // Pin HIGH pulls down the amp /EN lines
void turnOff()      { digitalWrite(RELAY_PIN, LOW); }
void turnOn()       { digitalWrite(RELAY_PIN, HIGH); }
void disable()      { digitalWrite(AMP_ENABLE_PIN, HIGH); }
void enable()       { digitalWrite(AMP_ENABLE_PIN, LOW); }

void limitedDelay(uint32_t ms, uint32_t max) { delay(ms > max ? max : ms ); }

PowerControl::PowerControl() {}

void PowerControl::begin() {
    turnOff();                  // Load the register before setting as output
    pinMode(RELAY_PIN, OUTPUT);
    turnOff();                  // Should be redundant - just in case someone mucks with the library

    pinMode(AMP_ENABLE_PIN, OUTPUT);
    disable(); 
    whenDisabled = millis();
    whenPowerOn = millis();     // Should init with first powerOn
    }

void PowerControl::powerOn() { 
    if (powerIsOn()) return;
    if (ampsEnabled()) ampDisable();  // We assume it's OK to power on immediately with EN low
    turnOn(); 
    whenPowerOn = millis();
    }

void PowerControl::powerOff() { 
    if (!powerIsOn()) return;
    if (ampsEnabled) ampDisable();
    uint32_t howLong = millis() - whenDisabled;
    if (howLong < enableDelay) limitedDelay(enableDelay - howLong, enableDelay);
    turnOff(); 
    }

void PowerControl::ampEnable() { 
    if (ampsEnabled()) return;
    uint32_t howLong = millis() - whenPowerOn;
    if (howLong < powerOnDelay) limitedDelay(powerOnDelay - howLong, powerOnDelay);
    enable(); 
    }

void PowerControl::ampDisable() { 
    if (!ampsEnabled()) return;
    disable(); 
    whenDisabled = millis();
    }
