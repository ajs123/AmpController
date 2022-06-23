// Setup menu

#include "Options.h"
#include "OptionsMenu.h"
extern U8G2 display;

// The menu is defined using macros, requiring some functions referred to in them to be forward declared
uint32_t remoteLearn(remoteCommands command);
result saveRemoteCodesOp(eventMask event, prompt &item);
result quitRemoteOp(eventMask event, prompt &item); 
void saveValues();


float f_maxVolume;
float f_maxInitialVolume, saved_f_maxInitialVolume;
float f_analogDigitalDifference;

void prepVolumeVals() {
    f_maxVolume = ampOptions.maxVolume * -0.5;
    f_maxInitialVolume = ampOptions.maxInitialVolume * -0.5;
    saved_f_maxInitialVolume = f_maxInitialVolume;
    f_analogDigitalDifference = ampOptions.analogDigitalDifference * -0.5;
}

void postVolumeVals() {
    Options::instance().maxVolume = f_maxVolume / -0.5;
    Options::instance().maxInitialVolume = f_maxInitialVolume / -0.5;
    Options::instance().analogDigitalDifference = f_analogDigitalDifference / -0.5;
}

result setVolumeVals(eventMask event) {
    switch (event) {
        case enterEvent:
            prepVolumeVals;
            break;
        case exitEvent:
            postVolumeVals;
            break;
    }
    return proceed;
}

// The volume at startup can be no higher than the absolute maximum.
// If the absolute maximum is lowered and then raised again without
// having edited the volume at startup, track appropriately.

result limitMaxInitialVolume(eventMask event) {
    f_maxInitialVolume = min(saved_f_maxInitialVolume, f_maxVolume);
    return proceed;
}

result handleMaxInitialVolume(eventMask event) {
    f_maxInitialVolume = min(f_maxInitialVolume, f_maxVolume);
    saved_f_maxInitialVolume = f_maxInitialVolume;
    return proceed;
}

/*
template<typename T>
class dBFormat : public menuField<T> {
public:
  using menuField<T>::menuField;
  Used printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr=0) override {
    menuField<T>::reflex=menuField<T>::target();
    prompt::printTo(root,sel,out,idx,len);
    bool ed=this==root.navFocus;
    out.print((root.navFocus==this&&sel)?(menuField<T>::tunning?'>':':'):' ');
    out.setColor(valColor,sel,menuField<T>::enabled,ed);
    char buffer[]="      ";
    sprintf(buffer, "%-5.1f", menuField<T>::reflex);
    out.print(buffer);
    out.setColor(unitColor,sel,menuField<T>::enabled,ed);
    print_P(out,menuField<T>::units(),len);
    return len;
  }
};
*/

// Specific to u8g2:
// This allows use of a different font for the menu title, and draws a fine line
// across the display beneath the title. 
class altTitle:public menu {
public:
  altTitle(constMEM menuNodeShadow& shadow):menu(shadow) {}
  idx_t ret;
  Used printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t p) override {
    if (idx < 0) {      // indicates that this is the title
        display.setFont(titleFontName);
        ret = menu::printTo(root,sel,out,idx,len,p);
        display.setFont(fontName);
        display.drawLine(0, fontY - 2, (int(U8_Width / fontX) * fontX - 1), fontY - 2);
    } else {
        ret = menu::printTo(root,sel,out,idx,len,p);
    }
    return ret;
    }
};

altMENU(altTitle, volumeMenu, "Volume", setVolumeVals, (eventMask)(enterEvent | exitEvent), noStyle, (Menu::_menuData|Menu::_canNav),
    altFIELD(decPlaces<1>::menuField, f_maxVolume, "Max", " dB", -40, 0, 0.5, 0, limitMaxInitialVolume, anyEvent, noStyle),
    altFIELD(decPlaces<1>::menuField, f_maxInitialVolume, "Max start", " dB", -40, 0, 0.5, 0, handleMaxInitialVolume, updateEvent, noStyle),
    altFIELD(decPlaces<1>::menuField, f_analogDigitalDifference, "A-D diff", " dB", -12, 12, 0.5, 0, doNothing, noEvent, noStyle),
    EXIT("<< BACK")
    );

char analogBuf[MAX_LABEL_LENGTH + 1];
char digitalBuf[MAX_LABEL_LENGTH + 1];

void padString(char * string, const uint8_t length) {
    uint8_t currentLength = strlen(string);
    if (currentLength >= MAX_LABEL_LENGTH) return;
    uint8_t len = min(length, MAX_LABEL_LENGTH);
    memset(string + currentLength, 32, MAX_LABEL_LENGTH - currentLength);
}

void fixString(char * string, const char * defaultLabel) {
    if (string[0] == 32) {
        strncpy(string, defaultLabel, MAX_LABEL_LENGTH);
        return;
    }
    uint8_t currentLength = strlen(string);
    for (uint8_t i = currentLength - 1; i > 0; i--) {
        if (string[i] != 32) break;
        string[i] = 0;
    }
}

void prepLabels() {
    strncpy(analogBuf, ampOptions.analogLabel, MAX_LABEL_LENGTH);
    padString(analogBuf, MAX_LABEL_LENGTH);
    strncpy(digitalBuf, ampOptions.digitalLabel, MAX_LABEL_LENGTH);
    padString(digitalBuf, MAX_LABEL_LENGTH);
}

void postLabels() {
    fixString(analogBuf, "ANALOG");
    strncpy(ampOptions.analogLabel, analogBuf, MAX_LABEL_LENGTH);
    fixString(digitalBuf, "DIGITAL");
    strncpy(ampOptions.digitalLabel, digitalBuf, MAX_LABEL_LENGTH);
}

result fixLabels(eventMask event) {
    switch(event) {
        case enterEvent:
            prepLabels();
            break;
        case exitEvent:
            postLabels(); 
            break;
    }
    return proceed;
}

const char * const uCase[] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789*-"};
altMENU(altTitle, labelMenu, "Input labels", fixLabels, (eventMask)(enterEvent | exitEvent), wrapStyle, (Menu::_menuData|Menu::_canNav),
    EDIT("Analog ", analogBuf, uCase, doNothing, noEvent, noStyle),
    EDIT("Digital ", digitalBuf, uCase, doNothing, noEvent, noStyle),
    EXIT("<< BACK")
    );


/* Not in use
//customizing a prompt look!
//by extending the prompt class
class altPrompt:public prompt {
public:
  altPrompt(constMEM promptShadow& p):prompt(p) {}
  Used printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr) override {
    root.active();
    char buffer[20];
    snprintf(buffer, 20, "%d %d %d %d", sel, idx, len, panelNr);
    return out.printRaw(buffer,strlen(buffer));;
  }
};
*/


// Individual functions for the remote menu
result volPlusOp(eventMask event, prompt &item) {remoteLearn(REMOTE_VOLPLUS); return proceed;}
result volMinusOp(eventMask event, prompt &item) {remoteLearn(REMOTE_VOLMINUS); return proceed;}
result muteOp(eventMask event, prompt &item) {remoteLearn(REMOTE_MUTE); return proceed;}
result inputOp(eventMask event, prompt &item) {remoteLearn(REMOTE_INPUT); return proceed;}
result powerOp(eventMask event, prompt &item) {remoteLearn(REMOTE_POWER); return proceed;}

//MENU(remoteMenu, "Remote...", saveRemoteCodesOp, exitEvent, wrapStyle,
altMENU(altTitle,remoteMenu, "Remote", doNothing, noEvent, wrapStyle,(Menu::_menuData|Menu::_canNav),
    OP("Vol +", volPlusOp, enterEvent),
    OP("Vol -", volMinusOp, enterEvent),
    OP("Mute", muteOp, enterEvent),
    OP("Input", inputOp, enterEvent),
    OP("Power", powerOp, enterEvent),
    //EXIT("<< BACK")
    OP("<<DONE", saveRemoteCodesOp, enterEvent),
    OP("<<CANCEL", quitRemoteOp, enterEvent)
    );

// We need a function that provides a prompt and then invokes saveValues
// (or a submenu with confirmation). 
// It would be good to track any changes and ask for confirmation of EXIT.
altMENU(altTitle, ampSetup, "SETUP", doNothing, noEvent, wrapStyle, (Menu::_menuData|Menu::_canNav),
    SUBMENU(volumeMenu),
    SUBMENU(remoteMenu),
    SUBMENU(labelMenu),
    OP("SAVE", saveValues, enterEvent),
    EXIT("<< EXIT")
    );

// Define the input

Menu::keyMap joystickBtn_map[]={
 {BTN_SEL, defaultNavCodes[enterCmd].ch,INPUT_PULLUP} ,
 {BTN_UP, defaultNavCodes[upCmd].ch,INPUT_PULLUP} ,
 {BTN_DOWN, defaultNavCodes[downCmd].ch,INPUT_PULLUP}
};
keyIn<3> joystickBtns(joystickBtn_map);

MENU_INPUTS(in, &joystickBtns);

// NOTES:
// (1) This macro isn't very useful when there's just one input (e.g., requires two)
// (2) We might replace it with separate checks of the encoder and programmed navigation calls
MENU_OUTPUTS(out, MAX_DEPTH
  ,U8G2_OUT(display, OptionsMenu::colors, fontX, fontY, offsetX, offsetY, {0, 0, U8_Width/fontX, U8_Height/fontY})
  ,NONE
);

// And go

NAVROOT(nav, ampSetup, MAX_DEPTH, in, out);

void u8g2Confirm(const char * text, uint16_t duration) {
    display.clear();
    display.setDrawColor(1);
    display.setCursor(0, (U8_Height + fontY)/2);
    display.print(text);
    display.updateDisplay();
    delay(duration);
}

void saveValues() {
    ampOptions.save();
    u8g2Confirm("Saved options.", 3000);
    nav.doNav(downCmd);
}

result saveRemoteCodesOp(eventMask event, prompt &item) {
    Serial.println("Copying remote codes to options store");
    ourRemote.saveToOptions();
    Serial.flush();
    nav.doNav(escCmd);
    return proceed;
}

bool remoteInit = false;

// To do - Set the foreground/background colors
//      - Save new command codes to the options bank.
//          - Should that be done here, or at the end of the menu?
//          - If the latter, it could be done in the remote handler - as converse of load()
/**
 * @brief Learn an IR code for the designated function
 * 
 * @param cmd The command (enum to keep in sync with the remote handler)
 * @return ArduinoMenu result (always proceed)
 */
uint32_t remoteLearn(remoteCommands cmd) {
    display.clear();
    display.setDrawColor(1);
    display.setCursor(0, fontY);
    display.println(remoteCommandNames[cmd]);
    display.setCursor(0, 2 * fontY);
    display.println("Press a button...");
    display.updateDisplay();

    uint32_t command = ourRemote.getRawCommand(5000);

    display.setCursor(0, 3 * fontY);
    if (command) {
        display.printf("Got %0X", command);
        ourRemote.set(cmd, command);
    } else {
        display.println("Timeout");
    }
    display.updateDisplay();
    delay(2000);

    nav.doNav(upCmd);                   // Next item (with buttons, up is down);
    if (!command) nav.doNav(downCmd);   // ... or position for a retry
    //nav.idleOn();                     // Alt force redraw
    //nav.idleOff();
    return proceed;
}

result quitRemoteOp(eventMask event, prompt &item) {
    Serial.println("Restoring remote codes from options store");
    Serial.flush();
    ourRemote.loadFromOptions();    // Pull back the saved codes
    nav.doNav(escCmd); 
    return proceed;
}

void OptionsMenu::menu(U8G2 * _display) {
    //display = _display;
    //display.begin();
    display.setFont(fontName);  // Presently, display is a global 
    
    nav.timeOut = MENU_TIMEOUT;
    options->invertFieldKeys = true;
    joystickBtns.begin();
    nav.useUpdateEvent = true;

    do {
        nav.doInput();
        if (nav.changed(0)) {
            nav.doOutput();
            display.updateDisplay();
        } 
    } while( !nav.sleepTask );

    display.clear();
}

