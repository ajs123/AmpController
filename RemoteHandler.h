// Take input from the IR receiver

#pragma once

#include <Arduino.h>
#include <IRLibRecvPCI.h>         // Use the pin change interrupt receiver
#include <IRLibDecodeBase.h>      // Base class for the decoder
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

// Default remote codes
#define VOLPLUS_CMD     0x77E1507C // Apple remote UP
#define VOLMINUS_CMD    0x77E1307C //              DOWN
#define MUTE_CMD        0x77E1A07C //              PLAY/PAUSE
#define INPUT_CMD       0x77E1C07C //              INPUT

// How long after a full code is a repeat taken to be valid
constexpr int maxDelayBeforeRepeat = 1000;

typedef void cmdHandler_t();    // Command handlers take nothing and return nothing

// Command handlers are externals. They can also be called from other classes, such as the
// one that handles the control knob.
extern cmdHandler_t volPlus, volMinus, mute, input;

class Remote : public IRrecvPCI, IRdecode {

public:
    /**
     * @brief Constructor
     * @param pin Pin number
     */
    Remote(uint8_t pin) : IRrecvPCI(pin) {
        //Serial.printf("Remote constructor, pin %d\r\n", pin);
        menuItem = 0;
        #ifdef NRF52_SERIES
        dwt_enable();  // Provides proper micros() on nrf5
        #endif
    }     

private:

    // IRLib2 provides a protocol ID, command, and address (extended data). 
    // Our receiver adds the time (millis()) at which the command was received.
    // struct IRCommand_t {
    //     uint8_t protocol;
    //     uint32_t command;
    //     uint16_t address;
    //     uint32_t receivedTime;
    // };

    // Dispatch table for remote commands.
    struct cmdEntry_t {
        uint32_t command;           // The IR command. We don't bother here with protocol or extended data ("address") 
        bool repeatable;            // Whether repeat codes are acted upon
        const char* name;           // The readable name, used when training
        cmdHandler_t* handler;      // The callback
    };

    static const uint8_t tableLength = 4;
    cmdEntry_t cmdTable[tableLength] = {
        {VOLPLUS_CMD, true, "Vol+", volPlus},
        {VOLMINUS_CMD, true, "Vol-", volMinus},
        {MUTE_CMD, false, "Mute", mute},
        {INPUT_CMD, false, "Input", input}
        };

    uint32_t receivedTime;
    bool repeat;
    uint32_t lastCommand;
    uint32_t lastProtocol;
    uint32_t lastReceiveTime;
    uint8_t menuItem;

    /**
     * @brief Look for a command in the dispatch table and call the correspondng funciton.
     * @param command the received IR command
     * @param repeat the command was a repeat (key held)
     * @return true if there was a match in the table
     */
    bool dispatch(uint32_t command, bool repeat);

    /**
     * @brief Turn any special repeat code into a duplicate of the original code, if 
     * received soon after the original.
     * Presently, handles only the NEC 0xFFFFFFFF repeat code.
     * @return true if a repeat was found
     */
    bool handleRepeats();

public:
    /**
     * @brief Checks for any command received and invokes the corresponding handler.
     * 
     */
    void Task();

    /**
     * @brief Starts the receiver listenting
     * 
     */
    void listen();

    /**
     * @brief Clears any pending received codes and stops listening. 
     * 
     */
    void stopListening();

    /**
     * @brief Ensures that there is nothing in the input buffer, waits for a fresh command (button press), 
     * and identifies the command received. Does not handle repeat codes. Used when learning a new remote.
     * @param pcommand - pointer to the receive buffer
     * @param wait (ms) - how long to wait for the initial button press
     * @return command received.  
     */
    bool getRawCommand(uint16_t wait);

    /**
     * @brief Checks for anything received, handles any special repeat codes, and re-enables the input.
     * Used in normal "always listening" operation.
     * @param pcommand - pointer to the receive buffer
     * @return command received. 
     */
    bool getCommand();

    // These support IR code learning: Scrolling through a menu of named commands, and capturing the codes.

    /**
     * @brief Learns an IR code. 
     * Waits to see a button press, then loads the first code seen into the dispatch table.
     * @param item the item number in the dispatch table
     * @param wait how long to wait (md)
     * @return command received.
     */
    bool learn(uint8_t item, uint16_t wait);

    /**
     * @brief Go to the next or previous item in the dispatch table.
     * @param forward true means next. 
     * @param wrap wrap around at either end of the list
     * @return the now-current item number
     */
    uint8_t nextMenuItem(bool forward, bool wrap);

    /**
     * @brief Provide the name of the current item in the command table
     * @return The item name 
     */
    const char * currentItemName();
};