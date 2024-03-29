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
        displayUpdate();
    }

    void AmpDisplay::displayUpdate() {
        if (autoRefresh) {
            display->updateDisplay();
            newContent = false;
        } else {
            newContent = true;
        }
    }

    void AmpDisplay::refresh() {
        if (newContent) {
            display->updateDisplay();
            newContent = false;
        }
    }

    void AmpDisplay::setImmediateUpdate(bool immediate) {
        autoRefresh = immediate;
        if (autoRefresh && newContent) refresh();
    }

    void AmpDisplay::drawVolume()
    {
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

        // Update the display
        displayUpdate();

        #ifdef TRANSIENTVOLUME
        volumeShown = true;
        #endif
    }

    void AmpDisplay::eraseVolume()
    {
        if (muteState) return;
        eraseArea(wholeVolumeArea);
        volumeShown = false;
        displayUpdate();  // Just redraw (no)
    }

    const char sourceLabels[2][8] = {
         "ANALOG",
         "DIGITAL"
    };

    // This ought to guard against the change indicator being beyond the edge of the display
    // It should possibly handle the case of the source being undetermined
    void AmpDisplay::drawSource(bool changeInd)
    {
        char label[MAX_LABEL_LENGTH + 5];
        char * sourceLabel = (sourceState == source_t::Analog) ? ampOptions.analogLabel : ampOptions.digitalLabel;
        snprintf(label, MAX_LABEL_LENGTH + 4, "%s%s", sourceLabel, changeInd ? " >>>" : "");
        //snprintf(label, 12, "%s%s", sourceLabels[sourceState], changeInd ? "-->" : "");
        displayText(label, SOURCE_FONT, sourceArea, true);
        displayUpdate();
    }

    void AmpDisplay::volume(float vol)
    {
    //    if (volumeState != vol)
    //    {
            volumeState = vol;
            drawVolume();
    //    }
        wakeup();
    }

    void AmpDisplay::preset(uint8_t preset)
    {
        char buf[9];
        eraseArea(volLabArea);
        snprintf(buf, 9, "Preset %1d", preset);
        displayText(buf, MUTE_FONT, wholeVolumeArea, true);
        displayUpdate();
    }


    // @brief Displays a message in the message area.
    // Does not wake up the display from a dimmed state
    void AmpDisplay::displayMessage(const char * message) {
        displayText(message, MSG_FONT, messageArea, true);
        displayUpdate();   // Calling this directly avoids restoring full contrast
    }

    // @brief Displays a message in the specified area.
    void AmpDisplay::displayMessage(const char * message, areaSpec_t area) {
        displayText(message, MSG_FONT, area, true);
        displayUpdate();
    }

    // @brief Dims the display
    void AmpDisplay::dim() {
        if (dimState) return;
        display->setContrast(contrast(ampOptions.brightness.lowBrightness - 1));
        dimState = true;
    #ifdef TRANSIENTVOLUME
        if (!muteState) eraseVolume();
    #endif
    }

    bool AmpDisplay::dimmed() {
        return dimState;
    }

    void AmpDisplay::scheduleDim() {
    brightTime = millis();
    }

    void AmpDisplay::checkDim() {
    if (dimState) return;
    uint32_t currentTime = millis();
    if ((currentTime - brightTime) > ampOptions.dimTime * 1000) {
        dim();
        }
    }

    void AmpDisplay::undim() {
        display->setContrast(contrast(ampOptions.brightness.highBrightness));
        dimState = false;
    }
    
    void AmpDisplay::wakeup() {
    #ifdef TRANSIENTVOLUME
        if (!volumeShown) drawVolume();
    #endif
        display->setContrast(contrast(ampOptions.brightness.highBrightness));
        dimState = false;
        scheduleDim();
    }

    void AmpDisplay::volumeMode(bool dB)
    {
        // If changed, update the display
        if (dBDisplay != dB)
        {
            dBDisplay = dB;
            displayUpdate();
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
        //if (sourceState != source)
        //{
            sourceState = source;
            drawSource();
        //}
        wakeup();
    }

    void AmpDisplay::cueLongPress()
    {
        //eraseArea(sourceArea);
        drawSource(true);
        wakeup();
    }

    void AmpDisplay::cancelLongPress()
    {
        drawSource(false);
    }

    void AmpDisplay::eraseArea(areaSpec_t area) {
        display->setDrawColor(0);
        display->drawBox(area.XL, area.YT, area.XR - area.XL, area.YB - area.YT);
    }

    void AmpDisplay::displayText(const char *text, const uint8_t *font, areaSpec_t area, const bool erase, const uint8_t offset)
    {
        if (erase)
        {
            eraseArea(area);
        }

        if (!text[0]) return;

        display->setDrawColor(1);
        display->setFont(font);
        display->setFontPosBaseline();       // ArduinoMenu sets this to Bottom in the u8g2Out constructor :-(
        if (area.rJust)
        {
            uint8_t vsWidth = display->getStrWidth(text);
            //display->setCursor(area.XR - vsWidth, area.YB);
            display->setCursor(area.XR - vsWidth, area.YB + offset);
        }
        else
        {
            //display->setCursor(area.XL, area.YB);
            display->setCursor(area.XL, area.YB + offset);
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

        uint8_t leftWidth = max( ( (int) left * (int) (barWidth) ) / 100, 1);
        uint8_t rightWidth = max( ( (int) right * (int) (barWidth) ) / 100, 1);

        display->setDrawColor(1);
        display->drawBox(leftAreaXR - leftWidth, area.YT + 2, leftWidth, area.YB - area.YT - 2);
        display->drawBox(rightAreaXL, area.YT + 2, rightWidth, area.YB - area.YT - 2);
        displayUpdate();  // Re-draw only, without full refresh and wakeup
    }
