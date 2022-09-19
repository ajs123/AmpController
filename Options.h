// Options of potential interest to the end user
// Some are determined at build time. Others are (at least potentially) settable through the setup menu and are saved to flash.

#pragma once
#include <Arduino.h>
#include "Configuration.h"
#include <Adafruit_LittleFS.h>
#include <Adafruit_LittleFS_File.h>
#include <InternalFileSystem.h>

// Build time options

// The maximum volume that can be set under any circumstances. 
// In MiniDSP units of -0.5 dB.
// This might be set to other than 0 to preclude amplifier clipping or driver overextension.
// This will be removed in favor of a configurable option
const uint8_t MAXIMUM_VOLUME = 0;

// Minimum level in dB for the VU meter
const int8_t MINBARLEVEL = -60;

// Setup menu timeout (sec)
const uint16_t MENU_TIMEOUT = 120;

// Choose preset timeout (ms) - clicks of the remote preset button no more than this far apart count as preset selection
const uint32_t CHOOSE_PRESET_TIMEOUT = 1200;

// Clipping detector threshold and persistence
const float defaultClippingHeadroom = 6.0;
const uint32_t clipIndicatorTime = 500;

// Silence detection behavior
// If the trigger for the non-selected source is present, will silence on the selected source cause a source change?
const bool silenceSourceChange = false;

// Include our version as the BLE DIS firmware string
const bool setFirmwareString = false;   // The default bootloader version may be more helpful

// Default remote codes
// These are for a particular Apple remote. 
// Hex codes are MMMMCCRR
//      MMMM - Remote model
//      CC   - Command code
//      RR   - Individual remote (allows two remotes of the same model to work separately)
#define DEFAULT_VOLPLUS_CMD     0x77E1507C // Apple remote UP
#define DEFAULT_VOLMINUS_CMD    0x77E1307C //              DOWN
#define DEFAULT_MUTE_CMD        0x77E1A07C //              PLAY/PAUSE
#define DEFAULT_INPUT_CMD       0x77E1607C //              RIGHT
#define DEFAULT_POWER_CMD       0x77E1C07C //              MENU
#define DEFAULT_PRESET_CMD      0x77E1907C //              LEFT

// Remote commands and their names
// Tables in this file, and in the remote handler, are indexed by this enum.

enum remoteCommands {
    REMOTE_VOLPLUS = 0,
    REMOTE_VOLMINUS,
    REMOTE_MUTE,
    REMOTE_INPUT,
    REMOTE_POWER,
    REMOTE_PRESET,
    REMOTE_COMMAND_COUNT
};

constexpr char remoteCommandNames[REMOTE_COMMAND_COUNT][6] = {"Vol +", "Vol -", "Mute", "Input", "Power"};

// User-configurable (writable) options
// Defaults are set in the initialization of the variables.
// This is configured as a Meyer's singleton. 

const uint8_t MAX_LABEL_LENGTH = 12;
const uint8_t MAX_VARIABLE_SIZE = MAX_LABEL_LENGTH + 1;
const uint8_t MAX_FNAME_LENGTH = 10;

class Options {

    public:
        // The individual options are public, so they're accessed directly by any client.
        // NOTE: Slightly better practice would be to make all of these const, then
        // const_cast that away when truly needing to change them (i.e., in the options menu only).
        // The defaults for these options are set in their initialization. Any values
        // stored in flash overwrite the defaults.

        // For better or for worse, maximum volume options are in MiniDSP units (-0.5 dB).
        // Maximum volume at any time (protects the neighbors), in MiniDSP units
        uint8_t maxVolume = 0;

        // Maximum volume at startup (avoids getting blasted in the morning), in MiniDSP units
        uint8_t maxInitialVolume = 10;

        // Analog-digital volume difference in dB, to compensate for input level differences
        int8_t analogDigitalDifference = 0;

        // Headroom for the clipping indicator, in dB
        uint8_t clippingHeadroom = defaultClippingHeadroom;

        // Label for the analog input
        char analogLabel[MAX_LABEL_LENGTH + 1] = "ANALOG";

        // Label for the digital input
        char digitalLabel[MAX_LABEL_LENGTH + 1] = "DIGITAL";

        // Auto-off time, in minutes. 
        uint8_t autoOffTime = 30;  

        // Definition of silence - low enough to be inaudible; high enough to accomodate any noise from the analog source
        uint8_t silence = 140; // -70.0 dB

        // Remote codes
        uint32_t volPlusCmd = DEFAULT_VOLPLUS_CMD;
        uint32_t volMinusCmd = DEFAULT_VOLMINUS_CMD;
        uint32_t muteCmd = DEFAULT_MUTE_CMD;
        uint32_t inputCmd = DEFAULT_INPUT_CMD;
        uint32_t powerCmd = DEFAULT_POWER_CMD;

        // Display brightness
        typedef struct {
            uint8_t lowBrightness;
            uint8_t highBrightness;
        } dispBrightness_t;

        dispBrightness_t brightness = { 1, 8 };

        // Display dim time
        uint8_t dimTime = 5;

        static Options & instance() {
            static Options _instance;
            return _instance;
        }

    private:
        typedef struct {
            void * value;
            const char name[MAX_FNAME_LENGTH + 1];
            uint8_t size;
        } writableOption_t;

        // This holds a reference to each variable with its name in the filesystem and its size
        // To add a variable to nonvolatile storage, add it here.
        // There is no cleanup of nonvolatile storage, so if a name is changed or an entry is removed,
        // there will be an orphaned file in the filesystem.
        const writableOption_t optionTable[15] = {
            {&maxVolume,               "Vol_max",   sizeof(maxVolume)},
            {&maxInitialVolume,        "Vol_init",  sizeof(maxInitialVolume)},
            {&analogDigitalDifference, "AD_diff",   sizeof(analogDigitalDifference)},
            {&clippingHeadroom,        "Headroom",  sizeof(clippingHeadroom)},
            {&analogLabel,             "A_label",   sizeof(analogLabel)},
            {&digitalLabel,            "D_label",   sizeof(digitalLabel)},
            {&autoOffTime,             "Auto_off",  sizeof(autoOffTime)},
            {&silence,                 "Silence",   sizeof(silence)},
            {&volPlusCmd,              "Vol_plus",  sizeof(volPlusCmd)},
            {&volMinusCmd,             "Vol_minus", sizeof(volMinusCmd)},
            {&muteCmd,                 "Mute_cmd",  sizeof(muteCmd)},
            {&inputCmd,                "Input_cmd", sizeof(inputCmd)},
            {&powerCmd,                "Power_cmd", sizeof(powerCmd)},
            {&brightness,              "Brightness",sizeof(brightness)},
            {&dimTime,                 "Dim_time",  sizeof(dimTime)}
        };

        // The parameters are saved in a folder in the filesystem, just in case the device is used
        // for more than one thing
        const char * FOLDER = "AmpController/";

        // Maximum length of a full file path
        const uint8_t MAX_PATH_LENGTH = 64;

        // Private constructor
        Options() {};

    public:

        /**
         * @brief Initialize the filesystem and be sure the parameter folder is there.
         * @return true if successful
         */
        bool begin();

        /**
         * @brief Read values from flash
         * @return Filesystem error 
         */
        bool load();

        /**
         * @brief Save values to flash, if they've changed.
         * Change is defined as different from what's in flash, or not found in flash.
         * Accordingly, each option gets written on the first run.
         * Reading from flash may waste time, but it simplifies everything else.
         * 
         * @return Filesystem error
         */
        bool save();

        /**
         * @brief True if current values are different from what's in flash. Any
         * missing files or errors count as changed.
         */
        bool changed();


    private:
        /**
         * @brief Read a parameter from the filesystem, if a corresponding file exists
         * 
         * @param name the file name
         * @param value pointer to the value
         * @param length expected length (bytes) of the value
         * @return 0 - read, 1 - no file, -1 - couldn't open, -2 - unexpected length
         */
        int8_t readParamFile(const char * name, void * value, const uint8_t length);

        /**
         * @brief Write a parameter file to the filesystem
         * 
         * @param name the file name
         * @param value pointer to the value
         * @param length bytelength of the value
         * @return 0 - written, -1 - couldn't open, -2 - unexpected length
         */
        int8_t writeParamFile(const char * name, const void * value, uint8_t length);

        /**
         * @brief Construct a full filename from a parameter name
         */
        void makePath(char * path, const char * name, const uint8_t maxLen);

};