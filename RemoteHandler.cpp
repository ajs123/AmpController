// Take input from the IR receiver

#include <Arduino.h>
#include "RemoteHandler.h"

bool Remote::handleRepeats() {
    // NEC repeat flag (provided by the IRRemote library)
    if ((protocolNum == NEC) && 
        (lastProtocol == NEC) &&
        (value == REPEAT_CODE) &&
        (receivedTime - lastReceiveTime <= maxDelayBeforeRepeat)) {
            value = lastCommand;
            return true;
        }
    return false;
}

bool Remote::getCommand() {

    if (!getResults()) return false;

    receivedTime = millis();
    decode();
    enableIRIn();

    if (!protocolNum) return false;

    Serial.printf("getCommand got protocol %0X value %0X\r\n", protocolNum, value);

    repeat = handleRepeats();
    Serial.printf("Returning it after checking repeats as %0X\r\n", value);

    lastCommand = value;
    lastProtocol = protocolNum;
    lastReceiveTime = receivedTime;
    return true;
}

// Per the brief, this returns true if the comand was found in the table, even if
// it was a repeat for a non-repeatable command
bool Remote::dispatch(uint32_t command, bool repeat) {

    for (uint8_t i = 0; i < tableLength; i++) {
        if (command == cmdTable[i].command) {
            Serial.printf("Recognized %s\r\n", cmdTable[i].name);
            if (!repeat || cmdTable[i].repeatable) cmdTable[i].handler();
            return true;
        }
    }
    return false;
} 

void Remote::Task() {
    if (!getCommand()) return;
    Serial.printf("Task: got protocol %0X, command %0X\r\n", protocolNum, value);
    dispatch(value, repeat);
}

void Remote::listen() {
    enableIRIn();
}

void Remote::stopListening() {
    disableIRIn();
    getResults();
}

bool Remote::getRawCommand(uint16_t wait) {

    uint32_t startTime = millis();

    enableIRIn();
    while (!getResults()) {
        if (millis() - startTime > wait) return false;
    }
    decode();
    receivedTime = millis();

    return true;
}

bool Remote::learn(uint8_t item, uint16_t wait) {

    if (item > tableLength) return false;

    if (!getRawCommand(wait)) return false;
    cmdTable[item].command = value;
    return true;
}

uint8_t Remote::nextMenuItem(bool forward, bool wrap) {
    
    int8_t item = menuItem;
    if (forward) {
        wrap ? item++ % tableLength : min(item++, tableLength - 1);
    }
    else {
        wrap ? (item + tableLength - 1) % tableLength : max(item--, 0);
    }
    menuItem = item;
    return menuItem;
}

const char * Remote::currentItemName() {
    return cmdTable[menuItem].name;
}