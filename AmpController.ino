/*
 Example sketch for the MiniDSP 2x4HD library - developed by Dennis Frett
 */

#include <Arduino.h>
//#include <Usb.h>
#include <MiniDSP.h>
#include <U8g2lib.h>
//#include "newDisp.h"
#include "AmpDisplay.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C // For this hardware (nrf52840) should just be able to include Wire.h
#include <Wire.h>
#endif

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

//ORIG USB Usb;
USB thisUSB;
MiniDSP ourMiniDSP(&thisUSB);

// Display stuff
#define LOG_FONT u8g2_font_5x7_tr //u8g2_font_7x14_tf
#define LOG_WIDTH 25 
#define LOG_HEIGHT 9

// Specify the display
U8G2_SH1107_64X128_F_HW_I2C display(U8G2_R1, U8X8_PIN_NONE); // Adafruit OLED Featherwing
//U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE); // Generic 128 x 64 SH1106 display on hardware I2C
U8G2 * display_p = &display;

// Attach the AmpDisplay to the display
AmpDisplay AmpDisp(display_p);

// U8G2 log is used at startup
uint8_t u8log_buffer[LOG_WIDTH * LOG_HEIGHT];
U8G2LOG displog;

void DisplaySetup() {
  display.begin();
  display.setFontMode(0);
  AmpDisp.displayMessage("START");
  
  // Start the startup log display
  //displog.begin(display, LOG_WIDTH, LOG_HEIGHT, u8log_buffer);
  //displog.setLineHeightOffset(0);
  //displog.setRedrawMode(0);
  //display.setFont(LOG_FONT);
  //displog.println("Startup");
}



void OnMiniDSPConnected() {
  Serial.println("MiniDSP connected");
  //displog.println("MiniDSP connected.");
  AmpDisp.displayMessage("CONNECTED");
}

void OnVolumeChange(uint8_t volume) {
  Serial.println("Volume: " + String(volume));
  AmpDisp.volume(-volume/2.0);
  //displog.println("Volume " + String(volume));
}

void OnMutedChange(bool isMuted) {
  Serial.println("Muted status: " + String(isMuted ? "muted" : "unmuted"));
  Serial.flush();
  //displog.println(String(isMuted ? "Muted" : "Unmuted"));
  //if (isMuted) displayMessage("MUTE"); else displayMessage("UNMUTE");
}

void setup() {
  DisplaySetup();
  Serial.begin(115200);
  delay(200);
  //while(!Serial) delay(10);

  if(thisUSB.Init() == -1) {
    Serial.print(F("\r\nOSC did not start"));
    //displog.println("OSC did not start");
    while(1); // Halt
  }
  Serial.println(F("\r\nMiniDSP 2x4HD Library Started"));
  //displog.println(F("MiniDSP 2x4HD Library Started"));

  // Register callbacks.
  ourMiniDSP.attachOnInit(&OnMiniDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&OnVolumeChange);
  ourMiniDSP.attachOnMutedChange(&OnMutedChange);

  //Do we need an Init for ourMiniDSP??
  //ourMiniDSP.Init(0, 0, false);
  //Serial1.printf("MiniDSP is at address %u\r\n", ourMiniDSP.GetAddress());

}

void loop() {

  thisUSB.Task();

}
