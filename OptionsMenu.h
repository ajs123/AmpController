// Options menu
#pragma once

#include <Arduino.h>
#include "Options.h"
#include <menu.h>
#include <menuIO/u8g2Out.h>
// #include <menuIO/encoderIn.h>
// #include <menuIO/keyIn.h>
#include <menuIO/altKeyIn.h>
#include <menuIO/chainStream.h>
//#include <menuIO/serialOut.h>
//#include <menuIO/serialIn.h>

// Access the options
extern Options & ampOptions; // = Options::instance();

namespace OptionsMenu {

    #define BTN_SEL 5       // Temporary - until the encoder is defined
    #define BTN_UP 6 
    #define BTN_DOWN 9

    #define fontName u8g2_font_7x13_mf
    #define fontX 7
    #define fontY 16
    #define offsetX 0
    #define offsetY 3
    #define U8_Width 128
    #define U8_Height 64

    #define MAX_DEPTH 3     // Main, sub, item

    using namespace Menu;


    // define menu colors --------------------------------------------------------
    //each color is in the format:
    //  {{disabled normal,disabled selected},{enabled normal,enabled selected, enabled editing}}
    // this is a monochromatic color table
    const colorDef<uint8_t> colors[6] MEMMODE={
    {{0,0},{0,1,1}},//bgColor
    {{1,1},{1,0,0}},//fgColor
    {{1,1},{1,0,0}},//valColor
    {{1,1},{1,0,0}},//unitColor
    {{0,1},{0,0,1}},//cursorColor
    {{1,1},{1,0,0}},//titleColor
    };

    /**
     * @brief Provide the setup menu on the designated display
     * 
     */
    void menu(U8G2 * display);

};