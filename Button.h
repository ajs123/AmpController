// Button debounce and event detection

#include <Arduino.h>
#include "Configuration.h"

#pragma once

/**
 * Button events
 * 
 * * SHORT_PRESS - upon release, after a natural press of the button
 * * LONG_PRESS - upon release, after holding the button momentarily
 * * LONG_PRESS_PENDING - while the button is held long enough for LONG_PRESS
 * * FULL_HOLD - while the button has been held for an extended time
 * 
 * See the separate definitions of the applicable time periods
 */
enum buttonEvent_t {
    NONE,
    SHORT_PRESS,
    LONG_PRESS_PENDING,
    LONG_PRESS,
    FULL_HOLD
};

/**
 * Callbacks
 */
extern void shortPress();
extern void longPressPending();
extern void longPress();
extern void fullHold();

class Button {

const uint8_t MIN_PRESS_T = 50;     // ms
const uint8_t MIN_RELEASE_T = 100;
const uint16_t LONG_PRESS_T = 750;
const uint16_t FULL_HOLD_T = 4000;

public:
    /**
     * @brief Constructor
     */
    Button(uint32_t pin_);

    /**
     * @brief Check the button and return any event. Full hold is reported whenever the timing
     * criterion is met. Short press and long press are reported upon release.
     * 
     * @return buttonEvent_t event
     */
    buttonEvent_t update();

    /**
     * @brief Invoke functions according to button events
     * @return Event type
     */
    buttonEvent_t Task();

    /**
     * States for the debounce and event detect state machine.
     * 
     * * RELEASED - validated button released state 
     * * CLOSED - provisional closed state; will revert to RELEASED or move on to PRESSED
     * * PRESSED - validated button down state
     * * OPEN - provisional open state; will revert to PRESSED or move on to RELEASED
     */
    enum buttonState_t {
        RELEASED, 
        CLOSED,  
        PRESSED,  
        OPEN 
    };

private:

    uint32_t pin;
    buttonState_t buttonState;
    uint32_t lastChangeTime;
    bool fullHoldLock;

};