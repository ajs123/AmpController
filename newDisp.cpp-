#include "newDisp.h"

void newDisp::drawNum(uint8_t num) {
    display->setCursor(10, 20);
    display->print(num);
}
    void newDisp::volume(float vol)
    {
            char volString[8];
            bool dBDisplay = true;
            uint8_t vsWidth;

            if (dBDisplay)
        {
            // display is -xxx.x dB with dB half-sized at baseline
            snprintf(volString, 6, "%3.1f", vol);
            display->setFont(VOL_DB_FONT);
            vsWidth = display->getStrWidth(volString);
            display->setCursor(VOL_DB_RIGHT - vsWidth, VOL_BOTTOM);
            display->print(volString);
            display->setFont(DB_FONT);
            display->print("dB");
        }
        else
        {
            snprintf(volString, 3, "%u",  ((-vol/2)-MIN_VOLUME)/(MAX_VOLUME-MIN_VOLUME));
            display->setFont(VOL_PCT_FONT);
            vsWidth = display->getStrWidth(volString);
            display->setCursor(VOL_DB_RIGHT - vsWidth, VOL_BOTTOM);
            display->print(volString);
            display->setFont(PCT_FONT);
            display->print("%");
        }
        display->updateDisplay();
    }

    void newDisp::drawFrame()
    {
        // Clear the display buffer
        display->clear();

        // Draw whatever dividing lines, etc. we need
        display->drawFrame(1,1,127,63);
        // Nothing right now
    }
