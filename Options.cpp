// User-configurable (writable options)

#include "Options.h"

Adafruit_LittleFS_Namespace::File file(InternalFS);  // Stream class on the filesystem

bool Options::begin() {
  if (!InternalFS.begin()) return false;
  if (InternalFS.exists(FOLDER)) return true;
  if (InternalFS.mkdir(FOLDER)) return true;
  return false;
}

void Options::makePath(char * path, const char * name, const uint8_t maxLen) {
    strncpy(path, FOLDER, maxLen);
//    strncat(path, "/", maxLen); // FOLDER includes the "/"
    strncat(path, name, maxLen);
}

int8_t Options::readParamFile(const char * name, void * value, const uint8_t length) {
    char path[MAX_PATH_LENGTH];
    makePath(path, name, MAX_PATH_LENGTH);

    //Serial.printf("\nChecking for %s...\n", path);
    if (!InternalFS.exists(path)) return 1;
    if (!file.open(path, Adafruit_LittleFS_Namespace::FILE_O_READ)) return -1;

    uint8_t trimmedLength = min(length, MAX_VARIABLE_SIZE);
    uint8_t nRead = file.read(value, trimmedLength);
    file.close();
    //Serial.printf("Read %d bytes (expected %d).\n", nRead, trimmedLength); 
    if (nRead != trimmedLength) return -2;

    return 0;
}

int8_t Options::writeParamFile(const char * name, const void * value, uint8_t length) {
    char path[MAX_PATH_LENGTH];
    makePath(path, name, MAX_PATH_LENGTH);

    //Serial.printf("\nWriting %s...\n", path);
    if (InternalFS.exists(path)) InternalFS.remove(path);  // Remove and replace preferred over open and seek (?)
    if (!file.open(path, Adafruit_LittleFS_Namespace::FILE_O_WRITE)) return -1;

    uint8_t trimmedLength = min(length, MAX_VARIABLE_SIZE);
    uint8_t nWritten = file.write((const char *) value, trimmedLength);
    file.close();
    //Serial.printf("Wrote %d bytes (expected %d)\n", nWritten, trimmedLength);
    if (nWritten != trimmedLength) return -2;

    return 0;
}

bool Options::load() {
    bool error = false;
    for (uint8_t i = 0; i < sizeof(optionTable)/sizeof(writableOption_t); i++) {
        uint8_t intResult = readParamFile(optionTable[i].name, optionTable[i].value, optionTable[i].size);
        error |= (intResult < 0);
    }
    return !error;
}

void printHex(char * value, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        Serial.printf("%0X ", value[i]);
    }
    Serial.println("");
}

bool Options::changed() {
    char buf[MAX_VARIABLE_SIZE];
    //char bufPrint[MAX_VARIABLE_SIZE];
    //Serial.println("Checking for changes...");
    for (uint8_t i = 0; i < sizeof(optionTable)/sizeof(writableOption_t); i++) {
        uint8_t trimmedLength = min(optionTable[i].size, MAX_VARIABLE_SIZE);
        int8_t result = readParamFile(optionTable[i].name, &buf, trimmedLength);
        //Serial.printf("\nParameter %s:\nTable: ", optionTable[i].name);
        //memcpy(bufPrint, optionTable[i].value, optionTable[i].size);
        //printHex(bufPrint, optionTable[i].size);
        //Serial.print("Flash: ");
        //printHex(buf, optionTable[i].size);
        if (result || memcmp(buf, optionTable[i].value, optionTable[i].size)) {
            //Serial.printf("Change in %s\n", optionTable[i].name);
            return true; 
        }
    }
    return false;
}

bool Options::save() {
    char buf[MAX_VARIABLE_SIZE];
    bool error = false;

    for (uint8_t i = 0; i < sizeof(optionTable)/sizeof(writableOption_t); i++) {

        uint8_t trimmedLength = min(optionTable[i].size, MAX_VARIABLE_SIZE);

        // Check what's already in flash for this variable. 
        int8_t result = readParamFile(optionTable[i].name, &buf, trimmedLength);
        if (result < 0) {
            error = true;
            continue;
        }
        else if (result == 0) {
            //Serial.printf("\nsave() of %s found \n", optionTable[i].name);
            //Serial.print("Flash: ");
            //printHex(buf, optionTable[i].size);
            //Serial.print("Current: ");
            //char bufCurrent[MAX_VARIABLE_SIZE];
            //memcpy(bufCurrent, optionTable[i].value, optionTable[i].size);
            //printHex(bufCurrent, optionTable[i].size);
            // If what's written there is the same as the current value, nothing to write.
            if (!memcmp(buf, optionTable[i].value, optionTable[i].size)) continue;
        }
        result = writeParamFile(optionTable[i].name, optionTable[i].value, optionTable[i].size);
        error |= (result < 0);
    }
    return error;
}