// Options menu
#pragma once

#include <Arduino.h>
#include "Options.h"
#include "src/Menu/menu.h" // <menu.h>
#include "src/Menu/u8g2Out.h" //<menuIO/u8g2Out.h>
//#include "src/Menu/altKeyIn.h" //<menuIO/altKeyIn.h>
#include "src/Menu/MenuIO/chainStream.h" //<menuIO/chainStream.h>
#include "RemoteHandler.h"
#include "util.h"


namespace OptionsMenu {

    // The following gives a linker error because things in the .ino are in the global namespace.
    //Remote ourRemote = Remote::instance();

    #define fontName u8g2_font_7x13_mf
    //#define titleFontName u8g2_font_7x13_mf
    #define titleFontName u8g2_font_9x18_mf
    #define fontX 7
    #define fontY 16
    #define offsetX 0
    #define offsetY 0 //3
    #define U8_Width 128
    #define U8_Height 64

    #define MAX_DEPTH 3     // Main, sub, item

    using namespace Menu;

    // Each color is in the format:
    //  {{disabled normal,disabled selected},{enabled normal,enabled selected, enabled editing}}
    // this is a monochromatic color table
    const Menu::colorDef<uint8_t> colors[6] MEMMODE={
    {{0,0},{0,1,1}},//bgColor
    {{1,1},{1,0,0}},//fgColor
    {{1,1},{1,0,0}},//valColor
    {{1,1},{1,0,0}},//unitColor
    {{0,1},{0,0,1}},//cursorColor
    {{0,0},{0,1,1}},//titleColor (inverted - see menuIo.cpp at --->titleStart)
    };

    /// Provide a blocking version of the setup menu on the designated display.
    /// Depricated in favor of integration with the state machine
    void menu(U8G2 * display);  

    /// Set up and start the menu
    void begin();

    /// Process
    void task();

    /// Set the menu back to the top
    void reset();

    /// Cursor or value up
    void cursorUp();

    /// Cursor or value down
    void cursorDown();

    /// Enter
    void enter();
};