// Setup menu

#include "OptionsMenu.h"

extern U8G2 display;

extern void onMenuExit();   // Menu exit callback

namespace OptionsMenu {

    Remote & ourRemote = Remote::instance();
    Options & ampOptions = Options::instance();

    float f_maxVolume;
    float f_maxInitialVolume, saved_f_maxInitialVolume;
    float f_analogDigitalDifference;
    float f_clippingHeadroom;

    float f_silence;

    uint8_t fullExp;
    uint8_t dimExp;

    // Private utility functions and menu callbacks

    /**
     * @brief Learn an IR code for the designated function
     * 
     * @param cmd The command (enum to keep in sync with the remote handler)
     * @return ArduinoMenu result (always proceed)
     */
    uint32_t remoteLearn(remoteCommands command);

    /// @brief Copy remote codes to the options store
    result saveRemoteCodesOp(eventMask event, prompt &item);

    /// @brief Restore remote codes from the options store
    result quitRemoteOp(eventMask event, prompt &item); 

    /// @brief Save all options to flash
    void saveValues();

    /// @brief Prepare options for the volume menu
    void prepVolumeVals();

    /// @brief Post-processing of options after the volume menu
    void postVolumeVals();

    /// @brief Handle maxVolume interaction with other options
    result handleMaxVolume(eventMask event);

    /// @brief Handle maxInitialVolume interactions with other options
    result handleMaxInitialVolume(eventMask event);

    /// @brief main callback for the volume menu
    result setVolumeVals(eventMask event);

    /// @brief main callback for the auto off menu
    result setAutoOffVals(eventMask event);

    /// @brief handle changes to the high brightness setting
    result handlHighBrightness(eventMask event);

    /// @brief handle changes to the low brightness setting
    result handleLowBrightness(eventMask event);

    /// @brief main callback for the display menu
    result setDispVals(eventMask event);

    /// @brief Pad with spaces, so that the cursor can move past the end of the current string
    void padString(char * string, const uint8_t length);

    /// @brief Remove trailing spaces
    void fixString(char * string, const char * defaultLabel);

    /// @brief Prep labels for the menu
    void prepLabels();

    /// @brief Restore labels after the menu
    void postLabels();

    /// @brief Entry and exit callback for the labels menu
    result fixLabels(eventMask event);

    /// @brief Entry callback for the main setup menu
    result setupEntry(eventMask event);

    // Individual functions for the remote menu
    result volPlusOp(eventMask event, prompt &item) {remoteLearn(REMOTE_VOLPLUS); return proceed;}
    result volMinusOp(eventMask event, prompt &item) {remoteLearn(REMOTE_VOLMINUS); return proceed;}
    result muteOp(eventMask event, prompt &item) {remoteLearn(REMOTE_MUTE); return proceed;}
    result inputOp(eventMask event, prompt &item) {remoteLearn(REMOTE_INPUT); return proceed;}
    result powerOp(eventMask event, prompt &item) {remoteLearn(REMOTE_POWER); return proceed;}
    result presetOp(eventMask event, prompt &item) {remoteLearn(REMOTE_PRESET); return proceed;}

    char analogBuf[MAX_LABEL_LENGTH + 1];
    char digitalBuf[MAX_LABEL_LENGTH + 1];

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

    // NOTE: Specific to u8g2
    // This provides use of a different font for the menu title, and draws a fine line
    // across the display beneath the title. It also allows the menu title to be marked
    // when changes have been made, as a reminder to the user of the need to save them.
    class altTitle:public menu {
    public:
    altTitle(constMEM menuNodeShadow& shadow):menu(shadow), editFlag {false} {}
    idx_t ret;
    bool editFlag;
    char buf[33];
    Used printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t p) override {
        if (idx < 0) {      // indicates that this is the title
            display.setFont(titleFontName);
            if (editFlag) {
                strncpy(buf, getText(), 32);
                strncat(buf, " *", 32);
                ret = out.printRaw(buf, strlen(buf));
            } else {
                ret = menu::printTo(root,sel,out,idx,len,p);
            }
            display.setFont(fontName);
            display.drawLine(0, fontY - 2, (int(U8_Width / fontX) * fontX - 1), fontY - 2);
        } else {
            ret = menu::printTo(root,sel,out,idx,len,p);
        }
        return ret;
        }
    void edits(bool ed) { editFlag = ed; }
    };

    // //-----------------------------------------------
    // // For testing of events
    // //-----------------------------------------------
    // void showPath(navRoot& root) {
    //   Serial.print("nav level:");
    //   Serial.print(root.level);
    //   Serial.print(" path:[");
    //   for(int n=0;n<=root.level;n++) {
    //     Serial.print(n?",":"");
    //     Serial.print(root.path[n].sel);
    //   }
    //   Serial.println("]");
    // }

    // result showEvent(eventMask e,navNode& nav,prompt& item) {
    //   Serial.println();
    //   Serial.println("========");
    //   Serial.print("Event for target: 0x");
    //   Serial.println((long)nav.target,HEX);
    //   showPath(*nav.root);
    //   Serial.print(e);
    //   switch(e) {
    //     case noEvent://just ignore all stuff
    //       Serial.println(" noEvent");break;
    //     case activateEvent://this item is about to be active (system event)
    //       Serial.println(" activateEvent");break;
    //     case enterEvent://entering navigation level (this menu is now active)
    //       Serial.println(" enterEvent");break;
    //     case exitEvent://leaving navigation level
    //       Serial.println(" exitEvent");break;
    //     case returnEvent://TODO:entering previous level (return)
    //       Serial.println(" returnEvent");break;
    //     case focusEvent://element just gained focus
    //       Serial.println(" focusEvent");break;
    //     case blurEvent://element about to lose focus
    //       Serial.println(" blurEvent");break;
    //     case selFocusEvent://TODO:child just gained focus
    //       Serial.println(" selFocusEvent");break;
    //     case selBlurEvent://TODO:child about to lose focus
    //       Serial.println(" selBlurEvent");break;
    //     case updateEvent://Field value has been updated
    //       Serial.println(" updateEvent");break;
    //     case anyEvent:
    //       Serial.println(" anyEvent");break;
    //   }
    //   return proceed;
    // }
    // //------------------------------------------------

    // Provides a field with NONE in place of 0
    template<typename T>
    class offField:public menuField<T> {
    public:
    using menuField<T>::menuField;
    Used printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr=0) override {
        menuField<T>::reflex=menuField<T>::target();
        prompt::printTo(root,sel,out,idx,len);
        bool ed=this==root.navFocus;
        out.print((root.navFocus==this&&sel)?(menuField<T>::tunning?'>':':'):' ');
        out.setColor(valColor,sel,menuField<T>::enabled,ed);
        char buffer[]="      ";
        T value = menuField<T>::reflex;
        if (value == (T)0) {
            out.print(" NONE");
        } else {
            snprintf(buffer, 7, "%3d", menuField<T>::reflex);
            out.print(buffer);
        }
        out.setColor(unitColor,sel,menuField<T>::enabled,ed);
        if (value != (T)0) print_P(out,menuField<T>::units(),len);
        return len;
    }
    };

    // Build the menus

    altMENU(altTitle, volumeMenu, "Volume", setVolumeVals, (eventMask)(enterEvent | exitEvent), noStyle, (Menu::_menuData|Menu::_canNav),
        altFIELD(decPlaces<1>::menuField, f_maxVolume, "Max", " dB", -40, 0, 0.5, 0, handleMaxVolume, anyEvent, noStyle),
        altFIELD(decPlaces<1>::menuField, f_maxInitialVolume, "Max start", " dB", -40, 0, 0.5, 0, handleMaxInitialVolume, updateEvent, noStyle),
        altFIELD(decPlaces<1>::menuField, f_analogDigitalDifference, "A boost", " dB", 0, 12, 0.5, 0, doNothing, noEvent, noStyle),
        altFIELD(decPlaces<1>::menuField, f_clippingHeadroom, "Headroom", " dB", 0, 12, 1.0, 0, doNothing, noEvent, noStyle),
        EXIT("<< BACK")
        );

    altMENU(altTitle, autoOffMenu, "Auto off", setAutoOffVals, /*setupEntry, exitEvent*/ (eventMask)(enterEvent | exitEvent), noStyle, (Menu::_menuData|Menu::_canNav),
        altFIELD(offField, ampOptions.autoOffTime, "Auto off", " min", 0, 60, 5, 0, doNothing, noEvent, noStyle),
        altFIELD(decPlaces<1>::menuField, f_silence, "Level", " dB", -80, -30, 5, 0, doNothing, noEvent, noStyle),
        EXIT("<< BACK")
        );

    const char * const uCase[] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789*-"};
    altMENU(altTitle, labelMenu, "Input labels", fixLabels, (eventMask)(enterEvent | exitEvent), noStyle, (Menu::_menuData|Menu::_canNav),
        EDIT("Analog ", analogBuf, uCase, doNothing, noEvent, noStyle),
        EDIT("Digital ", digitalBuf, uCase, doNothing, noEvent, noStyle),
        EXIT("<< BACK")
        );

    altMENU(altTitle, displayMenu, "Display", setDispVals, (eventMask)(enterEvent | exitEvent), noStyle, (Menu::_menuData|Menu::_canNav),
        FIELD(fullExp, "Full", "", 1, 8, 1, 0, handlHighBrightness, anyEvent, noStyle),
        FIELD(dimExp, "Dim", "", 1, 8, 1, 0, handleLowBrightness, anyEvent, noStyle),
        FIELD(ampOptions.dimTime, "Time", " sec", 5, 30, 5, 0, doNothing, noEvent, noStyle),
        EXIT("<< BACK")
    );

    //Note: setupEntry on exit is a kludge to cover lack of enterEvent activation in the top level menu
    altMENU(altTitle,remoteMenu, "Remote", setupEntry, exitEvent, noStyle,(Menu::_menuData|Menu::_canNav),
        OP("Vol +", volPlusOp, enterEvent),
        OP("Vol -", volMinusOp, enterEvent),
        OP("Mute", muteOp, enterEvent),
        OP("Input", inputOp, enterEvent),
        OP("Preset", presetOp, enterEvent),
        OP("Power", powerOp, enterEvent),
        OP("<< KEEP", saveRemoteCodesOp, enterEvent),
        OP("<< CANCEL", quitRemoteOp, enterEvent)
        );

    // setupEntry isn't invoked because the top level has no enterEvent. 
    // We handle this by including setupEntry in the exitEvent callbacks for the sub-menus
    altMENU(altTitle, ampSetup, "SETUP", doNothing, noEvent, noStyle, (Menu::_menuData|Menu::_canNav),
    //altMENU(altTitle, ampSetup, "SETUP", setupEntry, enterEvent, noStyle, (Menu::_menuData|Menu::_canNav),
    //altMENU(altTitle, ampSetup, "SETUP", showEvent, anyEvent, noStyle, (Menu::_menuData|Menu::_canNav),
        SUBMENU(volumeMenu),
        SUBMENU(autoOffMenu),
        SUBMENU(displayMenu),
        SUBMENU(remoteMenu),
        SUBMENU(labelMenu),
        OP("SAVE", saveValues, enterEvent),
        EXIT("<< EXIT")
        );

    // No input, since we're driving through knob and button callbacks
    chainStream<0> in(NULL);

    // NOTE: This macro isn't very useful when there's just one output (e.g., requires two)
    MENU_OUTPUTS(out, MAX_DEPTH
    ,U8G2_OUT(display, colors, fontX, fontY, offsetX, offsetY, {0, 0, U8_Width/fontX, U8_Height/fontY})
    ,NONE
    );

    // And go

    NAVROOT(nav, ampSetup, MAX_DEPTH, in, out);

    // On entry to the main setup menu, check whether the current settings differ from what's in flash
    result setupEntry(eventMask event) {
        ampSetup.editFlag = ampOptions.changed();
        return proceed;
    }

    void prepVolumeVals() {
        f_maxVolume = ampOptions.maxVolume * -0.5;
        f_maxInitialVolume = ampOptions.maxInitialVolume * -0.5;
        f_clippingHeadroom = (float)ampOptions.clippingHeadroom;
        saved_f_maxInitialVolume = f_maxInitialVolume;
        f_analogDigitalDifference = ampOptions.analogDigitalDifference * 0.5; // Analog boost is positive
    }

    // Setting ampSetup.exits should no longer be necessary
    void postVolumeVals() {
        //Serial.printf("Vol vals %f, %f, %f\n", f_maxVolume, f_maxInitialVolume, f_analogDigitalDifference);
        uint8_t maxVolume = f_maxVolume / -0.5;
        if (maxVolume != ampOptions.maxVolume) {
            ampOptions.maxVolume = maxVolume;
            //ampSetup.edits(true);
        }
        uint8_t maxInitialVolume = f_maxInitialVolume / -0.5;
        if (maxInitialVolume != ampOptions.maxInitialVolume) {
            ampOptions.maxInitialVolume = maxInitialVolume;
            //ampSetup.edits(true);
        }
        int8_t analogDigitalDifference = f_analogDigitalDifference / 0.5;
        //Serial.printf("AD diff was %d, now %d\n", ampOptions.analogDigitalDifference, analogDigitalDifference);
        if (analogDigitalDifference != ampOptions.analogDigitalDifference) {
            ampOptions.analogDigitalDifference = analogDigitalDifference;
            //ampSetup.edits(true);
        }
        ampOptions.clippingHeadroom = (uint8_t)f_clippingHeadroom;
        //Note: kludge to cover lack of enterEvent activation in the top level menu
        setupEntry(enterEvent);
    }

    result setVolumeVals(eventMask event) {
        //Serial.flush();
        switch (event) {
            case enterEvent:
                prepVolumeVals();
                break;
            case exitEvent:
                postVolumeVals();
                break;
        }
        return proceed;
    }

    // The maximum volume can't be set above the compile-time maximum.
    // And the volume at startup can't be higher than the absolute maximum.
    // If the absolute maximum is lowered and then raised again without
    // having edited the volume at startup, track appropriately.

    result handleMaxVolume(eventMask event) {
        f_maxVolume = min(f_maxVolume, MAXIMUM_VOLUME / -0.5);
        f_maxInitialVolume = min(saved_f_maxInitialVolume, f_maxVolume);
        return proceed;
    }

    result handleMaxInitialVolume(eventMask event) {
        f_maxInitialVolume = min(f_maxInitialVolume, f_maxVolume);
        saved_f_maxInitialVolume = f_maxInitialVolume;
        return proceed;
    }

    // uint8_t log2(const uint8_t num) {
    //     if (num < 2) return 0;
    //     uint8_t log2 = 0;
    //     uint8_t n = num;
    //     while (n = n >> 1) log2++;
    //     return log2;
    // }

    result setAutoOffVals(eventMask event) {
        switch (event) {
            case enterEvent:
                f_silence = ampOptions.silence * -0.5;
                break;
            case exitEvent:
                ampOptions.silence = f_silence / -0.5;
                setupEntry(exitEvent);
                break;
        }
        return proceed;
    }

    uint8_t initFullExp {8};
    uint8_t initDimExp {1};

    result setDispVals(eventMask event) {
        switch(event) {
            case enterEvent:
                fullExp = ampOptions.brightness.highBrightness;
                dimExp = ampOptions.brightness.lowBrightness;
                initFullExp = fullExp;
                initDimExp = dimExp;
                break;
            case exitEvent:
                ampOptions.brightness.highBrightness = fullExp;
                ampOptions.brightness.lowBrightness = dimExp;
                break;
        }
        return proceed;
    }

    inline void setContrast(uint8_t level) { display.setContrast(contrast(level)); }

    result handlHighBrightness(eventMask event) {
        switch(event) {
            case focusEvent:
                initDimExp = dimExp;
                break;
            case enterEvent:
            case updateEvent:
                dimExp = min(initDimExp, fullExp);
                //display.setContrast(1 << (fullExp - 1));
                setContrast(fullExp);
        }
        return proceed;
    }

    result handleLowBrightness(eventMask event) {
        switch(event) {
            case focusEvent:
                initFullExp = fullExp;
                break;
            case exitEvent:
                fullExp = max(initFullExp, dimExp);
                //display.setContrast(1 << (fullExp - 1));
                setContrast(fullExp);
                break;
            case enterEvent:
            case updateEvent:
                fullExp = max(initFullExp, dimExp);
                //display.setContrast(1 << (dimExp - 1));
                setContrast(dimExp);
                break;
        }
        return proceed;
    }

    void padString(char * string, const uint8_t length) {
        uint8_t currentLength = strlen(string);
        if (currentLength >= MAX_LABEL_LENGTH) return;  // Strings already longer than the limit are left as is
        uint8_t desiredLength = min(length, MAX_LABEL_LENGTH);
        memset(string + currentLength, 32, desiredLength - currentLength);
    }

    // Remove trailing spaces
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
        if (strncmp(analogBuf, ampOptions.analogLabel, MAX_LABEL_LENGTH)) {
            strncpy(ampOptions.analogLabel, analogBuf, MAX_LABEL_LENGTH);
            ampSetup.edits(true);
        }
        fixString(digitalBuf, "DIGITAL");
        if (strncmp(digitalBuf, ampOptions.digitalLabel, MAX_LABEL_LENGTH)) {
            strncpy(ampOptions.digitalLabel, digitalBuf, MAX_LABEL_LENGTH);
            ampSetup.edits(true);
        }
        setupEntry(enterEvent);     // Top-level menu has no enterEvent
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
        setupEntry(enterEvent);     // Top-level menu has no enterEvent
        return proceed;
    }

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
        ampSetup.edits(false);
        nav.doNav(downCmd);
    }

    result saveRemoteCodesOp(eventMask event, prompt &item) {
        //Serial.println("Copying remote codes to options store");
        ourRemote.saveToOptions();
        //Serial.flush();
        nav.doNav(escCmd);
        ampSetup.edits(true);
        return proceed;
    }

    bool remoteInit = false;

    // To do - Set the foreground/background colors
    //      - Save new command codes to the options bank.
    //          - Should that be done here, or at the end of the menu?
    //          - If the latter, it could be done in the remote handler - as converse of load()

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
        //Serial.println("Restoring remote codes from options store");
        Serial.flush();
        ourRemote.loadFromOptions();    // Pull back the saved codes
        nav.doNav(escCmd); 
        return proceed;
    }

    void menu(U8G2 * _display) {
        //display = _display;
        //display.begin();
        display.setFont(fontName);  // Presently, display is a global 
        
        nav.timeOut = MENU_TIMEOUT;
        options->invertFieldKeys = true;
        //joystickBtns.begin();
        nav.useUpdateEvent = true; 
        nav.idleOff();

        do {
            nav.doInput();
            if (nav.changed(0)) {
                nav.doOutput();
                display.updateDisplay();
            } 
        } while( !nav.sleepTask );

        display.clear();
    }

    void begin() {
        display.setFont(fontName);  // Presently, display is a global 
        nav.timeOut = MENU_TIMEOUT;
        options->invertFieldKeys = false;
        nav.useUpdateEvent = true; 
        nav.idleOff();
        ampSetup.edits(false);  // Should no longer be needed
        setupEntry(enterEvent);
        }

    void task() {
        bool changed;
        if (nav.changed(0)) {
            nav.doOutput();
            display.updateDisplay();
        }
        if (nav.sleepTask) onMenuExit();
    }

    void cursorUp() { nav.doNav(upCmd); }
    void cursorDown() { nav.doNav(downCmd); }
    void enter() { nav.doNav(enterCmd); }
    void reset() { nav.reset(); }

}