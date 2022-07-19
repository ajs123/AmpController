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

// Time (ms) after wich the volume indicator will go away and the diplay will dim
const uint16_t DIM_TIME = 5 * 1000;   

// Minimum level in dB for the VU meter
const uint8_t MINBARLEVEL = -60;

// Setup menu timeout (sec)
const uint16_t MENU_TIMEOUT = 120;

// Default remote codes
// These are for a particular Apple remote. The last two hex digits are the individual remote ID.
#define DEFAULT_VOLPLUS_CMD     0x77E1507C // Apple remote UP
#define DEFAULT_VOLMINUS_CMD    0x77E1307C //              DOWN
#define DEFAULT_MUTE_CMD        0x77E1A07C //              PLAY/PAUSE
#define DEFAULT_INPUT_CMD       0x77E1C07C //              INPUT
#define DEFAULT_POWER_CMD       0x00

// Remote commands and their names
// Tables in this file, and in the remote handler, are indexed by this enum.

enum remoteCommands {
    REMOTE_VOLPLUS = 0,
    REMOTE_VOLMINUS,
    REMOTE_MUTE,
    REMOTE_INPUT,
    REMOTE_POWER,
    REMOTE_COMMAND_COUNT
};

constexpr char remoteCommandNames[REMOTE_COMMAND_COUNT][6] = {"Vol +", "Vol -", "Mute", "Input", "Power"};

// User-configurable (writable) options
// Defaults are set in the initialization of the variables.
// This is configured as a Meyer's singleton. Other classes can access it using
// Options & options = Options::instance();
// In namespaces cordoned off by namespace{} only, the reference in the main .ino remains
// visible so either a unique name or an extern is needed.

const uint8_t MAX_LABEL_LENGTH = 12;
const uint8_t MAX_VARIABLE_SIZE = MAX_LABEL_LENGTH + 1;
const uint8_t MAX_FNAME_LENGTH = 10;

class Options {

    //Remote & ourRemote = Remote::instance();

    public:
        // The individual options are public, so they're accessed directly (not thread safe) by any client.
        // The defaults for these options are set in initialization.

        // Maximum volume at any time (protects the neighbors)
        uint8_t maxVolume = 0;

        // Maximum volume at startup (avoids getting blasted in the morning)
        uint8_t maxInitialVolume = 10;

        // Analog-digital volume difference, to compensate for input level differences
        uint8_t analogDigitalDifference = 0;

        // Label for the analog input
        char analogLabel[MAX_LABEL_LENGTH + 1] = "ANALOG";

        // Label for the digital input
        char digitalLabel[MAX_LABEL_LENGTH + 1] = "DIGITAL";

        // Auto-off time, in minutes. May need to revisit the units.
        uint8_t autoOffTime = 30;  

        // Remote codes
        uint32_t volPlusCmd = DEFAULT_VOLPLUS_CMD;
        uint32_t volMinusCmd = DEFAULT_VOLMINUS_CMD;
        uint32_t muteCmd = DEFAULT_MUTE_CMD;
        uint32_t inputCmd = DEFAULT_INPUT_CMD;
        uint32_t powerCmd = DEFAULT_POWER_CMD;

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
        const writableOption_t optionTable[11] = {
            {&maxVolume, "Vol_max", sizeof(maxVolume)},
            {&maxInitialVolume, "Vol_init", sizeof(maxInitialVolume)},
            {&analogDigitalDifference, "AD_diff", sizeof(analogDigitalDifference)},
            {&analogLabel, "A_label", sizeof(analogLabel)},
            {&digitalLabel, "D_label", sizeof(digitalLabel)},
            {&autoOffTime, "Auto_off", sizeof(autoOffTime)},
            {&volPlusCmd, "Vol_plus", sizeof(volPlusCmd)},
            {&volMinusCmd, "Vol_minus", sizeof(volMinusCmd)},
            {&muteCmd, "Mute_cmd", sizeof(muteCmd)},
            {&inputCmd, "Input_cmd", sizeof(inputCmd)},
            {&powerCmd, "Power_cmd", sizeof(powerCmd)}
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
         * 
         * @return true if successful
         */
        bool begin();

        /**
         * @brief Read values from flash
         * 
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

    private:
        /**
         * @brief Read a parameter from the filesystem, if a corresponding file exists
         * 
         * @param name the file name
         * @param value pointer to the value
         * @param length bytelength of the value
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
         * 
         */
        void makePath(char * path, const char * name, const uint8_t maxLen);

};