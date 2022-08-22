// Display for the amp controller
// Simple layout: Volume in dB or %, or MUTE, in a large font, source indicator, and an area used for 
// a VU meter or messages (typically for debugging)

#pragma once

#include "Options.h"
#include <U8g2lib.h>
#include "src/UHS/MiniDSP.h"
#include "util.h"

// Display settings of most interest to the user
#define CONTRAST_FULL 128
#define CONTRAST_DIM 16

// Options
#define DBDISPLAY true  // Display volume in dB (instead of %). Potentially a live option.
#define TRANSIENTVOLUME // Display volume only transient, when changed

// Font choices
#define VOL_PCT_FONT    u8g2_font_helvR24_tr
#define PCT_FONT        u8g2_font_helvR18_tr
#define VOL_DB_FONT     u8g2_font_helvR24_tr
#define DB_FONT         u8g2_font_helvR14_tf
#define MUTE_FONT       u8g2_font_helvR24_tr
#define MSG_FONT        u8g2_font_helvR08_tr
#define SOURCE_FONT     u8g2_font_helvB12_tf

// Display zones
struct areaSpec_t {
  uint8_t XL;   // X left and right
  uint8_t XR;
  uint8_t YT;   // Y top and bottom (top of screen is 0)
  uint8_t YB;
  bool rJust;   // Right justify? - This could be an enum for left, right, center...
} ;

// Text areas {XLeft, XRight, YTop, YBottom, rightJustify}
// Text is aligned at the bottom. The height should be at least the height of the A or 1 of
// the font used, with extra (bounding box height) if any text with descenders is displayed.
// If displaying text with descenders, specify an offset in displayText to make room.
constexpr areaSpec_t sourceArea =       {  0, 127,  0, 13, false};
constexpr areaSpec_t volumeArea =       {  0, 100, 20, 48, true};
constexpr areaSpec_t volLabArea =       {101, 127, 20, 48, false};
constexpr areaSpec_t wholeVolumeArea =  {  0, 127, 20, 48, false};
constexpr areaSpec_t messageArea =      {  0, 127, 53, 63, false};

class AmpDisplay {

protected:
    U8G2 * display;

    // Potentially live options
    bool dBDisplay = DBDISPLAY;      // Show dB (true) or %

    // State - supports updates only when necessary
    uint8_t muteState {3};                      // Muted? 0 = no; 1 = yes; 3 = undetermined
    float volumeState {-129};                   // dB.  Can be set below MIN_VOLUME to denote as-yet-undetermined.
    source_t sourceState {source_t::Unset};     // Current source. 
    bool dimState {false};                              // True if dimmed.
    uint32_t brightTime {false};                        // Time when dim timer was reset
    bool volumeShown {false};                   // Enables selective re-draw of volume in display wakeup 

    bool newContent {false};
    bool autoRefresh {false};

public:

    // Constructor
    AmpDisplay(U8G2 * u8g2)
    {
        display = u8g2;
        muteState = 3;
        volumeState = -129;
        sourceState = source_t::Unset;
        dimState = false;
        brightTime = millis();
        volumeShown = false;
        newContent = false;
        autoRefresh = false;
    }

    AmpDisplay(U8G2 * u8g2, float vol, bool mute, source_t input, bool dB)
    {
        display = u8g2;
        dBDisplay = dB;
        sourceState = input;
        muteState = mute;
        volumeState = vol;
    }
    
    // @brief Display volume, in % or dB.
    // This updates the volume state and as needed updates and wakes the display
    void volume(float vol);

    // @brief Display mode - sets volume display for % or dB
    void volumeMode(bool dB);
    void volumeMode(); // toggles dB/% mode

    // @brief Display mute
    // This sets the mute state and as needed updates and wakes the display
    void mute(bool isMuted);

    // @brief Display source
    // This sets the source state and as needed updates and wakes the display
    void source(source_t thesource);

    // @brief Set max volume for % mode
    void setMaxVolume(float max);

    // @brief See if anything needs to be done (dim, return to default, etc.)
    // NOT IMPLEMENTED - CURRENTLY, THINGS LIKE DIMMING ARE DONE BY THE APPLICATION
    void task();

    // @brief Draw the display background
    // Draws any dividers, etc. that persist
    void drawFrame();

    // @brief Display a text string in the default message area
    void displayMessage(const char * message);

    // @brief Display a text string in the selected area
    void displayMessage(const char * message, areaSpec_t area);

    // @brief Bar graph in the specified area
    // Levels are integer percent of full width
    void displayLRBarGraph(uint8_t leftLevel, uint8_t rightLevel, areaSpec_t area);

    // @brief Reset the dimming timer
    void scheduleDim();

    // @brief Check the dimming timer and dim if needed
    void checkDim();

    // @brief Dim the display
    void dim();

    // @brief Check if the display is dimmed
    bool dimmed();

    // @brief Un-dim the display, without restoring the volume indicator, or scheduling dimming
    void undim();

    // @brief Wake up the display, restoring the volume indicator as needed and scheduling dimming
    void wakeup();

    // @brief Cue a pending long press
    void cueLongPress();

    // @brief Never mind the long press
    void cancelLongPress();

    /**
     * @brief Set the update mode. When set, displayUpdate() will immediately
     * send the draw buffer to the display. When unset (default), displayUpdate() will note the need
     * for a refresh but a call to refresh() will be required to copy the buffer.
     * This provides better control over timing when multiple sources such as callbacks
     * modify the display. 
     * 
     * @param immediate
     */
    void setImmediateUpdate(bool immediate);

    /**
     * @brief Refresh the display if needed, by sending the draw buffer to the display.
     */
    void refresh();

private:

    Options & ampOptions = Options::instance();      // Access the options store

    // These are the max and min in dB that can be set in the minidsp
    const uint8_t MAX_VOLUME = 0; 
    const uint8_t MIN_VOLUME = -128;

    // @brief Display a text string in a specified area
    void displayText(const char * text, const uint8_t * font,  areaSpec_t area, const bool erase, const uint8_t offset = 0);

    // @brief Draw the volume indicator (including mute)
    // erase causes the field to be erased instead of redrawn
    void drawVolume();

    // @brief Erase the volume indicator
    void eraseVolume();

    // @brief Erase a text area
    void eraseArea(areaSpec_t area);

    /**
     * @brief Draw the source indicator
     * 
     * @param changeInd True if a change is pending - provides visual feedback for a button hold
     */
    void drawSource(bool changeInd = false);

    /**
     * @brief Update the display. According to the update mode, either copy the draw buffer
     * to the display, or note that refresh() needs to do so when called.
     */
    void displayUpdate();
};
