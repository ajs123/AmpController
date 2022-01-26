
#pragma once

#include <u8g2lib.h>

#define VOL_DB_RIGHT 108
#define DB_LEFT 110
#define VOL_BOTTOM 50

#define MIN_VOLUME -128
#define MAX_VOLUME 0

#define VOL_PCT_FONT u8g2_font_helvR18_tr
#define PCT_FONT u8g2_font_helvR14_tr
#define VOL_DB_FONT u8g2_font_helvR18_tr
#define DB_FONT u8g2_font_helvR08_tf

class newDisp {

protected:
    U8G2 * display;

    // Constructor associates the u8g2 instance with the display
public:
    newDisp(U8G2 * u8g2) {
        display = u8g2;
    }

    // Display volume, in % or dB.
    // This wakes the display and shows volume
    void volume(float vol);

    // Draw a number and refresh

    void drawNum(uint8_t num);
    void drawFrame();

};