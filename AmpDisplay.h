// Display for the amp controller
// Simple layout: Volume in dB or %, or MUTE, in a large font, and source indicator (line or toslink)
// The display should dim shortly after a volume or source change

#pragma once

#include <U8g2lib.h>

// Display settings of most interest to the user
#define BRIGHT_TIME (10 * 1000) // 10 seconds
#define CONTRAST_FULL 128
#define CONTRAST_DIM 32

// Options
#define DBDISPLAY true

// Display settings relating to layout and other detail
#define MAX_VOLUME 0 // These are the max and min in dB that can be set in the minidsp
#define MIN_VOLUME -128

// Font choices
#define VOL_PCT_FONT    u8g2_font_helvR24_tr
#define PCT_FONT        u8g2_font_helvR18_tr
#define VOL_DB_FONT     u8g2_font_helvR24_tr
#define DB_FONT         u8g2_font_helvR14_tf
#define MSG_FONT        u8g2_font_helvR08_tf

// Display zones
struct areaSpec_t {
  uint8_t XL;
  uint8_t XR;
  uint8_t YT;
  uint8_t YB;
  bool rJust;
} ;

constexpr areaSpec_t messageArea = {0, 127, 40, 63, false};
constexpr areaSpec_t volumeArea = {0, 100, 15, 39, true};
constexpr areaSpec_t volLabArea = {101, 127, 15, 39, false};

class AmpDisplay
{

protected:
    U8G2 * display;

    // Potentially live options
    bool dBDisplay = DBDISPLAY;      // Show dB (true) or %

    // State - supports updates only when necessary
    uint8_t muteState;          // Muted? 0 = no; 1 = yes; 3 = undetermined
    float volumeState;          // dB.  Can be set below MIN_VOLUME to denote as-yet-undetermined.
    uint8_t sourceState;        // Current source. 0 = analog, 1 = toslink, 3 = undetermined
    uint32_t dimTimer;

public:

    // Constructor
    AmpDisplay(U8G2 * u8g2)
    {
        display = u8g2;
    }

    AmpDisplay(U8G2 * u8g2, float vol, bool mute, uint8_t source, bool dB)
    {
        display = u8g2;
        dBDisplay = dB;
        sourceState = source;
        muteState = mute;
        volumeState = vol;
    }
    
    // Display volume, in % or dB.
    // This updates the volume state and as needed updates and wakes the display
    void volume(float vol);

    // Display mode - sets volume display for % or dB
    void volumeMode(bool dB);
    void volumeMode(); // toggles dB/% mode

    // Display mute
    // This sets the mute state and as needed updates and wakes the display
    // Note: There is no unmute() or mute(false) because volume() does this
    void mute();

    // Set max volume for % mode
    void setMaxVolume(float max);

    // Set source - 0 = analog, 1 = toslink
    // This sets the source state and as needed updates and wakes the display
    void setSource(uint8_t source);

    // See if anything needs to be done (dim, return to default, etc.)
    void task();

    // Draw the display background
    // Draws any dividers, etc. that persist
    void drawFrame();

    // @brief Display a text string in the message area
    void displayMessage(const char * message);

private:

    // @brief Display a text string in a specified area
    void displayText(const char * text, const uint8_t * font,  areaSpec_t area, const bool erase);

    // Draw the volume indicator (including mute)
    void drawVolume();

    // Draw the input indicator
    void drawInput();

    // Update the display
    void displayUpdate();

    // Padded, right-justified string for overwriting of a text field
    //void snRJust(const char * string, char * padded, const uint8_t len, const uint8_t fieldWidth, const uint8_t spaceWidth);

};
