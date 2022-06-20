// Setup menu

#include "OptionsMenu.h"

// Define the menu

void saveValues() {
    ampOptions.save();
}

// The volume fields need to be redefined to be other than unsigned byte
// - or - provide a custom display object to show dB as the unsigned number is changed
MENU(volumeMenu, "Volume...", doNothing, noEvent, noStyle,
    FIELD(ampOptions.maxVolume, "Max", "u", 0, 40, 1, 1, doNothing, noEvent, noStyle),
    FIELD(ampOptions.maxInitialVolume, "Max start", "u", 0, 40, 1, 1, doNothing, noEvent, noStyle),
    FIELD(ampOptions.analogDigitalDifference, "A-D diff", "u", 0, 12, 1, 1, doNothing, noEvent, noStyle),
    EXIT("<< BACK")
    );

MENU(ampSetup, "Setup...", doNothing, noEvent, wrapStyle,
    SUBMENU(volumeMenu),
    //SUBMENU(remoteMenu),
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
extern U8G2 display;
MENU_OUTPUTS(out, MAX_DEPTH
  ,U8G2_OUT(display, OptionsMenu::colors, fontX, fontY, offsetX, offsetY, {0, 0, U8_Width/fontX, U8_Height/fontY})
  ,NONE
);

// And go

NAVROOT(nav, ampSetup, MAX_DEPTH, in, out);

void OptionsMenu::menu(U8G2 * _display) {
    //display = _display;
    //display.begin();
    display.setFont(fontName);
    nav.timeOut = 20;
    joystickBtns.begin();

    do {
        nav.doInput();
        if (nav.changed(0)) {
            nav.doOutput();
            display.updateDisplay();
        } 
    } while( !nav.sleepTask );
}

