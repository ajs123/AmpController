// The big knob - really a simple wrapper around the Adafruit nrf52840 hardware rotary encoder

#include <RotaryEncoder.h>
#include "Configuration.h"

extern void onKnobTurned(int8_t change);

class Knob : public HwRotaryEncoder  {

public:
    Knob(uint8_t pinA, uint8_t pinB) : HwRotaryEncoder(), _pinA{pinA}, _pinB{pinB} { }
    
    // Initialize and also start
    void begin() { 
        HwRotaryEncoder::begin(_pinA, _pinB); 
        start();
        //RotaryEncoder.begin(_pinA, _pinB);
        //RotaryEncoder.start(); 
        };

    // Check for any change
    // This needs a way to ensure that _abs can't ever get beyond its range
    int8_t change() { 
        int8_t raw = static_cast<int8_t>(read()) + bit0Change;
        int8_t delta = raw / 2 ;
        bit0Change = raw - ( delta * 2 );
        return min( max(delta, -maxChange), maxChange); 
        };

    // Check for change and invoke the handler
    void task() {
        int32_t delta = change(); 
        //auto delta = RotaryEncoder.read() / 2;
        if (delta != 0) {
            onKnobTurned(delta); 
        };
    }

private:
    uint8_t _pinA;
    uint8_t _pinB;
    int8_t bit0Change {0};
    const int8_t maxChange {10};
};