    
#include "Button.h"

// Switch

Switch::Switch(uint32_t pin_) {
    pin = pin_;
    pinMode(pin, INPUT_PULLUP);
    validatedState = false;
    lastContactState = false;
    lastChangeTime = millis();
    lastStateChangeTime = lastChangeTime;
    timeInValidatedState = 0;
}

void Switch::checkPin() {

    bool currentContactState = !digitalRead(pin);
    uint32_t currentTime = millis();

    // If different from the last sample, note the time and the state.
    if (currentContactState != lastContactState) {
        lastChangeTime = currentTime;
        lastContactState = currentContactState;
        return;
    }

    // We're here if at least the last two samples were the same.
    // If also the same as the validated state, then we're persisting pressed or released.
    if (currentContactState == validatedState) {
        timeInValidatedState = currentTime - lastStateChangeTime;
        return;
    }

    // Otherwise, if it's been long enough since the last change,
    // we have a new validated state.
    if ((currentTime - lastChangeTime) > MIN_STABLE_T) {
        validatedState = currentContactState;
        lastStateChangeTime = currentTime;
        timeInValidatedState = 0;
        return;
    }

    // We're here if the last two samples were the same but it hasn't been long enough, 
    // so remain in the current state.
    timeInValidatedState = currentTime - lastStateChangeTime;
    return;
}

bool Switch::switchClosed() {
    checkPin();                   // We could do this selectively, based on how long since the last sample.
    return validatedState;
}

uint32_t Switch::closedFor() {
    checkPin();                  // We could do this selectively, based on how long since the last sample.
    return validatedState ? timeInValidatedState : 0 ;
}

// Button

Button::Button(uint32_t pin_) : Switch(pin_) {
        buttonState = RELEASED;
    };


// The button FSM could be implemented with an array of pointers to state functions corresponding to the elements
// of the state enum. But with only four states, it's easier to read this way!
Button::buttonEvent_t Button::update() {

    checkPin();
    switch (buttonState) {
        case RELEASED:
            if (validatedState) {
                buttonState = PRESSED;
            }
            return NONE;
        case PRESSED:
            if (!validatedState) {
                buttonState = RELEASED;
                return SHORT_PRESS;
            }
            if (timeInValidatedState > LONG_PRESS_T) {
                buttonState = LONG_PRESSED;
                return LONG_PRESS_PENDING;
            }
            return NONE;
        case LONG_PRESSED:
            if (!validatedState) {
                buttonState = RELEASED;
                return LONG_PRESS;
            }
            if (timeInValidatedState > FULL_HOLD_T) {
                buttonState = HELD;
                return FULL_HOLD;
            }
            return NONE;
        case HELD:
            if (!validatedState) {
                buttonState = RELEASED;
                }
            return NONE;
    };
    return NONE;        // This should not actually be reachable.
};

Button::buttonEvent_t Button::Task() {
    buttonEvent_t event = update();
    if (event >= EVENT_COUNT) return NONE;
    if (!dispatchTable[event]) return NONE;
    dispatchTable[event]();
    // switch (event) {
    //     case SHORT_PRESS:
    //         shortPress();
    //         break;
    //     case LONG_PRESS_PENDING:
    //         longPressPending();
    //         break;
    //     case LONG_PRESS:
    //         longPress();
    //         break;
    //     case FULL_HOLD:
    //         fullHold();
    //         break;
    // };
    return event;
}
