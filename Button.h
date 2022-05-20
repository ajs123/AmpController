// Button debounce and event detection

#include <Arduino.h>
#include "Configuration.h"

#pragma once

/**
 * Callbacks
 */
extern void shortPress();
extern void longPressPending();
extern void longPress();
extern void fullHold();

/**
 * @brief Provides a simple debounced switch
 */
class Switch {

// Debounce parameters
const uint8_t MIN_STABLE_T = 50;     // ms

public:
    /**
     * @brief Constructor
     */
    Switch(uint32_t pin_);


    /**
     * @brief Get the switch state
     * 
     * @return true for closed or false for open
     */
    bool switchClosed();

    /**
     * @brief Check if the switch is closed, and for how long
     * @return number of ms closed, or 0 for open
     */
    uint32_t closedFor();

protected:
    bool validatedState;            // Current validated state
    uint32_t currentTime;
    uint32_t lastStateChangeTime;   // Last millis() at which the validated state changed
    uint32_t timeInValidatedState;  

    /**
     * @brief Update the switch state
     */
    void checkPin();


private:
    uint32_t pin;
    bool lastContactState;        
    uint32_t lastChangeTime;        // Last millis() at which contact state changed
};

class Button : public Switch {

const uint16_t LONG_PRESS_T = 750;
const uint16_t FULL_HOLD_T = 4000;

public:
    /**
     * @brief Constructor
     */
    Button(uint32_t pin_);

/**
 * Button events
 * 
 * See the separate definitions of the applicable time periods
 */
    enum buttonEvent_t {
        NONE,
        SHORT_PRESS,        // Reported upon release, after a natural press of the button
        LONG_PRESS_PENDING, // Button held long enough to qualify as a long press (LONG_PRESS_T)
        LONG_PRESS,         // Reported upon release after a long press
        FULL_HOLD,          // Button held long enough to qualify as a full (long) hold (FULL_HOLD_T)
        EVENT_COUNT
    };

    /**
     * @brief The button handler state machine.
     * 
     * @return buttonEvent_t event
     */
    buttonEvent_t update();
    enum buttonState_t {
        RELEASED, 
        PRESSED,
        LONG_PRESSED,
        HELD
    };

    /**
     * @brief Invoke functions according to button events
     * @return Event type
     */
    buttonEvent_t Task();


private:

    buttonState_t buttonState;
    uint32_t lastChangeTime;
    bool fullHoldLock;

    //Dispatch table - must be in the same order as the buttonEvent_t enum
    typedef void cmdHandler_t();    // Command handlers take nothing and return nothing

    const cmdHandler_t * dispatchTable[EVENT_COUNT] = {
        0,
        shortPress,
        longPressPending,
        longPress,
        fullHold
    };

};