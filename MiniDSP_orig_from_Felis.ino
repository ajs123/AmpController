/*
 Example sketch for the MiniDSP 2x4HD library - developed by Dennis Frett
 */

#include <Arduino.h>
//#include <Usb.h>
#include "MiniDSP.h"
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C // For this hardware (nrf52840) should just be able to include Wire.h
#include <Wire.h>
#endif

// Satisfy the IDE, which needs to see the include statment in the ino too.
////#ifdef dobogusinclude
//#include <spi4teensy3.h>
//#endif
//#include <SPI.h>

// **TODO** Needs re-definition of Serial for debug output (and/or using the OLED display)

//ORIG USB Usb;
USBHost thisUSB;
MiniDSP ourMiniDSP(&thisUSB);

// Display stuff
#define LOG_FONT u8g2_font_5x7_tr //u8g2_font_7x14_tf
#define LOG_WIDTH 25 
#define LOG_HEIGHT 9
U8G2_SH1107_64X128_F_HW_I2C display(U8G2_R1, U8X8_PIN_NONE);
uint8_t u8log_buffer[LOG_WIDTH * LOG_HEIGHT];
U8G2LOG displog;


void DisplaySetup() {
  display.begin();
  //display.setContrast(128);
  //display.setFontMode(1); // "Transparent": Character background not drawn (since we clear the display anyway)
  
  // Start the startup log display
  displog.begin(display, LOG_WIDTH, LOG_HEIGHT, u8log_buffer);
  displog.setLineHeightOffset(0);
  displog.setRedrawMode(0);
  display.setFont(LOG_FONT);
  displog.println("Startup");
  //display.drawLog(0, 23, displog);			// draw the log content on the display
}

void OnMiniDSPConnected() {
  Serial1.println("MiniDSP connected");
  displog.println("MiniDSP connected.");
}

void OnVolumeChange(uint8_t volume) {
  //Serial1.println("Volume: " + String(volume));
  displog.println("Volume " + String(volume));
}

void OnMutedChange(bool isMuted) {
  Serial1.println("Muted status: " + String(isMuted ? "muted" : "unmuted"));
  displog.println(String(isMuted ? "Muted" : "Unmuted"));
}
#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
#define Serial SERIAL_PORT_USBVIRTUAL
#endif

void setup() {
  DisplaySetup();
  Serial1.begin(115200);
#if !defined(__MIPSEL__)
  while(!Serial1); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
    displog.println("UART is also ready.");
#endif
  if(thisUSB.Init() == -1) {
    Serial1.print(F("\r\nOSC did not start"));
    displog.println("OSC did not start");
    while(1); // Halt
  }
  Serial1.println(F("\r\nMiniDSP 2x4HD Library Started"));
  displog.println(F("MiniDSP 2x4HD Library Started"));

  // Register callbacks.
  ourMiniDSP.attachOnInit(&OnMiniDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&OnVolumeChange);
  ourMiniDSP.attachOnMutedChange(&OnMutedChange);
}

void loop() {
  //ourMiniDSP.;
  //float vol = ourMiniDSP.getVolumeDB();
  thisUSB.Task();
  if (ourMiniDSP.connected()) displog.println("Connected"); else displog.println("Not connected");
  //displog.printf("Vol %f\r\n", vol);
  delay(1000);
}
