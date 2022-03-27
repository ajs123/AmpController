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

        if (muteState)
        {
            // display is -MUTE-
            displayText("Mute", MUTE_FONT, volumeArea, true);
            displayText(" ", DB_FONT, volLabArea, true);
        }
        else if (dBDisplay)
        {
            // display is -xxx.x dB with dB half-sized at baseline
            snprintf(volString, 7, "%3.1f", volumeState);
            displayText(volString, VOL_DB_FONT, volumeArea, true);
            displayText("dB", DB_FONT, volLabArea, true);
        }
        else
        {
            // display is xxx %
            uint8_t volPct = (volumeState - MIN_VOLUME) / (MAX_VOLUME - MIN_VOLUME) * 100;
            snprintf(volString, 7, "%3u", volPct);
            displayText(volString, VOL_PCT_FONT, volumeArea, true);
            displayText("%", PCT_FONT, volLabArea, true);
        }
        // Call displayUpdate to refresh, set to full brightness, and reset the timer
        //displayUpdate();

        // Update the display
        display->updateDisplay();

        #ifdef TRANSIENTVOLUME
        volumeShown = true;
        #endif
    }

    void AmpDisplay::eraseVolume()
    {
        if (muteState) return;
        // To keep any details in displayText, use it with a blank string to clear
        displayText("", VOL_DB_FONT, wholeVolumeArea, true);  
        volumeShown = false;
        display->updateDisplay();  // Just redraw (no)
    }

    // Moved to drawVolume
    void drawMute()
    {
        // Write MUTE or its glyph to the volume display area
        // Call displayUpdate to refresh, set to full brightness, and reset the timer
    }

    void AmpDisplay::drawSource()
    {
        switch (sourceState)
        {
            case Analog:
                displayText("ANALOG", SOURCE_FONT, sourceArea, true);
                break;
            case Toslink:
                displayText("DIGITAL", SOURCE_FONT, sourceArea, true);
        }
        display->updateDisplay();
    }

    /* void AmpDisplay::displayUpdate()
    {
    #ifdef TRANSIENTVOLUME
        // Re-draw the volume if necessary
        if (!volumeShown) 
        {
            drawVolume(); // Includes a call to updateDisplay
        }
        // Otherwise, just write the buffer to the display
        else
        {
            display->updateDisplay();
        }
    #else
         // Write the buffer to the display
        display->updateDisplay();
    #endif
        // Set to full brightness
        display->setContrast(CONTRAST_FULL);
    } */

    void AmpDisplay::volume(float vol)
    {
        // If changed, save, call drawVolume with volume and dB and call displayUpdate
        if (volumeState != vol)
        {
            volumeState = vol;
            drawVolume();
        }
        wakeup();
    }


    // @brief Displays a message in the message area
    // Does not wake up the display from a dimmed state
    void AmpDisplay::displayMessage(const char * message) {
    displayText(message, MSG_FONT, messageArea, true);
    display->updateDisplay();   // Calling this directly avoids restoring full contrast
    }

    void AmpDisplay::dim() {
        display->setContrast(CONTRAST_DIM);
    #ifdef TRANSIENTVOLUME
        eraseVolume();
    #endif
    }

    void AmpDisplay::wakeup() {
    #ifdef TRANSIENTVOLUME
        if (!volumeShown) drawVolume();
    #endif
        display->setContrast(CONTRAST_FULL);
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

   void AmpDisplay::mute(bool isMuted)
    {
        if (muteState != isMuted)
        {
            muteState = isMuted ? 1 : 0;
            drawVolume();           // Presently, mute is part of the volume display
        }
        wakeup();
    }

    void AmpDisplay::setMaxVolume(float max)
    {
        // Set maxVolume
        // If changed and displaying %, call volume
    }

    void AmpDisplay::source(source_t source)
    {
        // Ignore if not 0 or 1
        if ((uint8_t) source > 1) return;
        if (sourceState != source)
        {
            sourceState = source;
            drawSource();
        }
        wakeup();
    }

    void AmpDisplay::displayText(const char *text, const uint8_t *font, areaSpec_t area, const bool erase)
    {
        if (erase)
        {
            display->setDrawColor(0);
            display->drawBox(area.XL, area.YT, area.XR - area.XL, area.YB - area.YT);
        }

        if (!text[0]) return;

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

    void AmpDisplay::displayLRBarGraph(uint8_t left, uint8_t right, areaSpec_t area)
    {
        // Erase the area
        display->setDrawColor(0);
        display->drawBox(area.XL, area.YT, area.XR - area.XL, area.YB - area.YT);

        // The bar graphs extend left and right from the center of the area
        // There's a two-pixel gap in the center
        // The area is assumed to have an even width
        uint8_t barWidth = (area.XR - area.XL + 1) / 2 ; // Assumes total area width is an even number
        uint8_t leftAreaXR = area.XL + barWidth  - 2; 
        uint8_t rightAreaXL = leftAreaXR + 2;

        // Get bar widths
        uint8_t leftWidth = max( ( (int) left * (int) (barWidth) ) / 100, 1);
        uint8_t rightWidth = max( ( (int) right * (int) (barWidth) ) / 100, 1);

        // Draw the bars
        display->setDrawColor(1);
        display->drawBox(leftAreaXR - leftWidth, area.YT + 2, leftWidth, area.YB - area.YT - 2);
        display->drawBox(rightAreaXL, area.YT + 2, rightWidth, area.YB - area.YT - 2);
        display->updateDisplay();  // Re-draw only, without full refresh and wakeup
        //char bufStr[25];
        //snprintf(bufStr, 25, "L %6d, R%6d", leftWidth, rightWidth);
        //displayMessage(bufStr);
    }
