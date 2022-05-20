// Take input from the IR receiver

#pragma once

#include <Arduino.h>
#include <IRLibRecvPCI.h>         // Use the pin change interrupt receiver
//#include <IRLibDecodeBase.h>      // Base class for the decoder
                                  // Decoder protocols
#include <IRLib_P01_NEC.h>
#include <IRLib_P02_Sony.h>
// #include <IRLib_P03_RC5.h>
// #include <IRLib_P04_RC6.h>
// #include <IRLib_P05_Panasonic_Old.h>
// #include <IRLib_P06_JVC.h>
// #include <IRLib_P07_NECx.h>
// #include <IRLib_P08_Samsung36.h>
// #include <IRLib_P09_GICable.h>
// #include <IRLib_P10_DirecTV.h>
// #include <IRLib_P11_RCMM.h>
// #include <IRLib_P12_CYKM.h>
//#include <IRLib_HashRaw.h>       // If all we want is a unique code for each key
#include <IRLibCombo.h>            // Uses the first protocol that appears to match

typedef void cmdHandler_t();    // Command handlers take nothing and return nothing

// Command handlers are externals. They can also be called from other classes, such as the
// one that handles the control knob.
extern cmdHandler_t volPlus, volMinus, mute, input;

class remoteReceiver {

    /**
     * @brief Construct a new remoteReceiver object.
     * 
     */
    remoteReceiver() {
        dwt_enable();
    }

private:
    // IRLib2 provides a protocol ID, command, and address (extended data). 
    // Our receiver adds the time (millis()) at which the command was received.
    struct IRCommand_t {
        uint8_t protocol;
        uint32_t command;
        uint32_t address;
        uint32_t receivedTime;
    };

    // Dispatch table for remote commands.
    struct cmdEntry_t {
        uint32_t command;           // The IR command. We don't bother here with protocol or extended data ("address") 
        const char* name;           // The readable name, used when training
        cmdHandler_t* handler;      // The callback
    };

    cmdEntry_t cmdTable[4] = {
        {0, "Vol+", volPlus},
        {0, "Vol-", volMinus},
        {0, "Mute", mute},
        {0, "Input", input}
        };

    IRCommand_t lastCommand;


public:
    /**
     * @brief IR command type - protocol, command, address
     * 
     */

    /**
     * @brief Checks for any command received and invokes the corresponding handler.
     * 
     */
    void Task();

    /**
     * @brief Ensures that there is nothing in the input buffer, waits for a fresh command (button press), 
     * and identifies the command received. Does not handle repeat codes. Used when learning a new remote.
     * @param wait (ms) - how long to wait for the initial button press
     * @return command received. Protocol 0, command 0, address 0 denotes nothing received 
     */
    IRCommand_t getRawCommand(uint16_t wait);

    /**
     * @brief Checks for anything received, handles any special repeat codes, and re-enables the input.
     * Used in normal "always listening" operation.
     * @return command received. 
     */
    IRCommand_t getCommand();

    // These support IR code learning: Scrolling through a menu of named commands, and capturing the codes.

    /**
     * @brief Learns an IR code. 
     * Waits to see a button press, then loads the first code seen into the dispatch table.
     * @param item the item number in the dispatch table
     * @param wait how long to wait (md)
     * @return command received.
     */
    IRCommand_t learn(uint8_t item, uint16_t wait);

    /**
     * @brief Go to the next or previous item in the dispatch table.
     * @param forward true means next. 
     * @param wrap wrap around at either end of the list
     * @return the now-current item number
     */
    uint8_t nextCmd(bool forward, bool wrap);

    /**
     * @brief Provide the name of the current item in the command table
     * @return The item name 
     */
    char * currentCmd();
};