// Display for the amp controller
// Simple layout: Volume in dB or %, or MUTE, in a large font, and source indicator (line or toslink)
// The display should dim shortly after a volume or source change

#pragma once

#include <U8g2lib.h>

// Display settings of most interest to the user
#define BRIGHT_TIME (10 * 1000) // 10 seconds
#define CONTRAST_FULL 128
#define CONTRAST_DIM 0

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
#define MUTE_FONT       u8g2_font_helvR18_tr
#define MSG_FONT        u8g2_font_helvR08_tf
#define SOURCE_FONT     u8g2_font_helvR12_tf

// Display zones
struct areaSpec_t {
  uint8_t XL;   // X left and right
  uint8_t XR;
  uint8_t YT;   // Y top and bottom (top of screen is 0)
  uint8_t YB;
  bool rJust;   // Right justify? - This could be an enum for left, right, center...
} ;

constexpr areaSpec_t sourceArea = {0, 127, 0, 20, false};
constexpr areaSpec_t messageArea = {0, 127, 50, 63, false};
constexpr areaSpec_t volumeArea = {0, 100, 22, 48, true};
constexpr areaSpec_t volLabArea = {101, 127, 22, 48, false};

// Input icons
#define icons_binary_width 32
#define icons_binary_height 32
static unsigned char icons_binary[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x70, 0x30, 0x70, 0x30, 0xfc, 0x70, 0xfc, 0x70,
   0xfc, 0x71, 0xfc, 0x71, 0x8e, 0x71, 0x8e, 0x71, 0x8e, 0x71, 0x8e, 0x71,
   0x8e, 0x71, 0x8e, 0x71, 0x8e, 0x71, 0x8e, 0x71, 0xce, 0x71, 0xce, 0x71,
   0xfc, 0x71, 0xfc, 0x71, 0xf8, 0x70, 0xf8, 0x70, 0x30, 0x20, 0x30, 0x20,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x80, 0x81, 0x21,
   0xf8, 0xe0, 0x87, 0x71, 0xfc, 0xe1, 0x87, 0x71, 0xce, 0x71, 0x8e, 0x71,
   0x8e, 0x71, 0x8e, 0x71, 0x8e, 0x71, 0x8e, 0x71, 0x8e, 0x71, 0x8e, 0x71,
   0x8e, 0x71, 0x8e, 0x71, 0xfc, 0xf1, 0x8f, 0x71, 0xfc, 0xe0, 0x87, 0x71,
   0x70, 0xc0, 0x83, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Inputs
enum source_t { 
    Analog = 0,
    Toslink = 1,
    Unset = 3};

class AmpDisplay
{

protected:
    U8G2 * display;

    // Potentially live options
    bool dBDisplay = DBDISPLAY;      // Show dB (true) or %

    // State - supports updates only when necessary
    uint8_t muteState = 3;              // Muted? 0 = no; 1 = yes; 3 = undetermined
    float volumeState = -129;           // dB.  Can be set below MIN_VOLUME to denote as-yet-undetermined.
    source_t sourceState = Unset;       // Current source. 
    uint32_t dimTimer;

public:

    // Constructor
    AmpDisplay(U8G2 * u8g2)
    {
        display = u8g2;
    }

    AmpDisplay(U8G2 * u8g2, float vol, bool mute, source_t input, bool dB)
    {
        display = u8g2;
        dBDisplay = dB;
        sourceState = input;
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
    void mute(bool isMuted);

    // Display source - 0 = analog, 1 = toslink
    // This sets the source state and as needed updates and wakes the display
    void source(source_t thesource);

    // Set max volume for % mode
    void setMaxVolume(float max);

    // See if anything needs to be done (dim, return to default, etc.)
    void task();

    // Draw the display background
    // Draws any dividers, etc. that persist
    void drawFrame();

    // @brief Display a text string in the message area
    void displayMessage(const char * message);

    // @brief Dim the display
    void dim();

    // @brief Wake up the display
    void wakeup();

private:

    // @brief Display a text string in a specified area
    void displayText(const char * text, const uint8_t * font,  areaSpec_t area, const bool erase);

    // Draw the volume indicator (including mute)
    void drawVolume();

    // Draw the input indicator
    void drawSource();

    // Update the display
    void displayUpdate();

};
