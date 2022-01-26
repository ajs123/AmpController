// AmpDisplay

#include "AmpDisplay.h"

    // Globals that are private to this translation unit
    uint8_t volSpaceWidth;      // Width of a space character - for padding strings
    uint8_t dBSpaceWidth;    

    void AmpDisplay::drawFrame()
    {
        // Clear the display buffer
        display->clear();

        // Draw whatever dividing lines, etc. we need
        display->updateDisplay();
    }

    void AmpDisplay::drawVolume()
    {
        // Write new volume indicator, or mute, to the volume display area
        // Volume is always in dB, displayed according to the dB boolean

        // Display in the selected format.
        // Include the dB or % indicator. If %, calculate from dB and min/max
        char volString[8];
        uint8_t vsWidth;

#define VOL_DB_RIGHT 98
#define DB_LEFT 100
#define VOL_BOTTOM 50

        // NEED TO CLEAR THE FIELD, or find font settings to properly overwrite empty areas
        // setFontMode(/*isTransparent*/ 0); when initializing
        // find the width of " " when initializing
        // left-pad strings with leading spaces to get just past x=0 
        //      (it appears that setCursor allows values < 0)

        if (muteState)
        {
        }
        else if (dBDisplay)
        {
            // display is -xxx.x dB with dB half-sized at baseline
            snprintf(volString, 7, "%3.1f", volumeState);
            display->setFont(VOL_DB_FONT);
            vsWidth = display->getStrWidth(volString);
            //display->setCursor(VOL_DB_RIGHT - vsWidth, VOL_BOTTOM);
            //display->print(volString);
            displayText(volString, VOL_DB_FONT, volumeArea, true);
            displayText("dB", DB_FONT, volLabArea, true);
            //display->setFont(DB_FONT);
            //display->print("dB");
        }
        else
        {
            // display is xxx %
            snprintf(volString, 3, "%u", (volumeState - MIN_VOLUME) / (MAX_VOLUME - MIN_VOLUME));
            display->setFont(VOL_PCT_FONT);
            vsWidth = display->getStrWidth(volString);
            display->setCursor(VOL_DB_RIGHT - vsWidth, VOL_BOTTOM);
            display->print(volString);
            display->setFont(PCT_FONT);
            display->print("%");
        }
        // Call displayUpdate to refresh, set to full brightness, and reset the timer
        displayUpdate();
    }

    void drawMute()
    {
        // Write MUTE or its glyph to the volume display area
        // Call displayUpdate to refresh, set to full brightness, and reset the timer
    }

    void AmpDisplay::drawInput()
    {
        // Write the new source indicator to the display area
        // Call displayUpdate
    }

    void AmpDisplay::displayUpdate()
    {
        // Write the buffer to the display
        display->updateDisplay();
        // Set to full brightness
        display->setContrast(CONTRAST_FULL);
        // Reset the timer
        dimTimer = millis() + BRIGHT_TIME;
    }

    void AmpDisplay::volume(float vol)
    {
        // If changed, save, call drawVolume with volume and dB and call displayUpdate
        if (volumeState != vol)
        {
            volumeState = vol;
            drawVolume();
        }
    }


    void AmpDisplay::displayMessage(const char * message) {
    displayText(message, MSG_FONT, messageArea, true);
    display->updateDisplay();
    }

    void AmpDisplay::volumeMode(bool dB)
    {
        // If changed, update the display
        if (dBDisplay != dB)
        {
            dBDisplay = dB;
            display->updateDisplay();
        }
    }

    void AmpDisplay::volumeMode()
    {
        // Toggle dBDisplay
        dBDisplay = !dBDisplay;
        // Call drawVolume with volume and dB
        drawVolume();
    }

   void AmpDisplay::mute()
    {
        // If changed, call drawMute and call displayUpdate
        // Set mute to true
        if (!muteState)
        {
            muteState = true;
            drawVolume();
        }
    }

    void AmpDisplay::setMaxVolume(float max)
    {
        // Set maxVolume
        // If changed and displaying %, call volume
    }

    void AmpDisplay::setSource(uint8_t source)
    {
        // Ignore if not 0 or 1
        // Save source
        // If changed, call displaySource and updateDisplay
    }

    void AmpDisplay::displayText(const char *text, const uint8_t *font, areaSpec_t area, const bool erase)
    {
        if (erase)
        {
            display->setDrawColor(0);
            display->drawBox(area.XL, area.YT, area.XR, area.YB);
        }
        display->setDrawColor(1);
        display->setFont(font);
        if (area.rJust)
        {
            uint8_t vsWidth = display->getStrWidth(text);
            display->setCursor(area.XR - vsWidth, area.YB);
        }
        else
        {
            display->setCursor(area.XL, area.YB);
        }
        display->print(text);
    }
