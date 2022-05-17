    
#include "Button.h"

// Constructor
Button::Button(uint32_t pin_) {
        pin = pin_;
        pinMode(pin, INPUT_PULLUP);
        buttonState = RELEASED;
        lastChangeTime = millis();
        fullHoldLock = false;
    };

buttonEvent_t Button::update() {

    uint32_t currentTime = millis();
    int rawValue = digitalRead(pin);
    uint32_t howLong = currentTime - lastChangeTime;

    switch (buttonState) {
        case RELEASED:                              // Validated released state
            fullHoldLock = false;
            if (rawValue == LOW) {
                lastChangeTime = currentTime;
                buttonState = CLOSED;
                return NONE;
            } 
            // rawValue == HIGH
            return NONE;
        case CLOSED:                                // Transitional state between RELEASED and PRESSED
            if (rawValue == HIGH) {
                buttonState = RELEASED;             // Open again --> drop back
                return NONE;
            }
            // rawValue == LOW
            if (howLong > MIN_PRESS_T) {
                buttonState = PRESSED;              // Closed for min time --> valid PRESSED
                return NONE;
            }
            // rawValue == LOW for < MIN_PRESS_T
            return NONE;
        case PRESSED:                               // Validated button press.
            if (rawValue == LOW) {                   
                if (fullHoldLock) return NONE;      // Report full hold only once
                if (howLong > FULL_HOLD_T) {        // FULL_HOLD is returned while the button is still pressed
                    fullHoldLock = true;
                    return FULL_HOLD;
                }
                if (howLong > LONG_PRESS_T) {       // ...as is advance notice of a LONG_PRESS
                    return LONG_PRESS_PENDING;
                }
                return NONE;
            }
            // rawValue == HIGH
            lastChangeTime = currentTime;
            buttonState = OPEN;
            return NONE;
        case OPEN:
            if (rawValue == LOW) {
                buttonState = PRESSED;              // Closed again --> drop back
                return NONE;
            }
            //rawValue == HIGH
            if (howLong < MIN_RELEASE_T) return NONE;
            buttonState = RELEASED;                 // Open for min time --> valid RELEASED
            if (fullHoldLock) return NONE;          // Already reported FULL_HOLD, so nothing more
            if (howLong > LONG_PRESS_T) return LONG_PRESS;
            return SHORT_PRESS;
    }
    return NONE;    // This shouldn't be reachable. Should we be concerned that the compiler gives a warning in its absence?
}

buttonEvent_t Button::Task() {
    buttonEvent_t event = update();
    switch (event) {
        case SHORT_PRESS:
            shortPress();
            break;
        case LONG_PRESS_PENDING:
            longPressPending();
            break;
        case LONG_PRESS:
            longPress();
            break;
        case FULL_HOLD:
            fullHold();
            break;
    };
    return event;
}
