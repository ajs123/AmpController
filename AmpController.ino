#include <Arduino.h>
//#include <Usb.h>
#include <MiniDSP.h>
#include <U8g2lib.h>
#include "AmpDisplay.h"

#ifdef U8X8_HAVE_HW_I2C // For this hardware (nrf52840) should just be able to include Wire.h
#include <Wire.h>
#endif

#include <SPI.h>

//ORIG USB Usb;
USB thisUSB;
MiniDSP ourMiniDSP(&thisUSB);

// Display stuff
#define LOG_FONT u8g2_font_5x7_tr //u8g2_font_7x14_tf
#define LOG_WIDTH 25 
#define LOG_HEIGHT 9

#define DIM_TIME 5000   // ms to dim the display
uint32_t brightTime;

// Specify the display
U8G2_SH1107_64X128_F_HW_I2C display(U8G2_R1, U8X8_PIN_NONE); // Adafruit OLED Featherwing
//U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE); // Generic 128 x 64 SH1106 display on hardware I2C
U8G2 * display_p = &display;

// Buttons on the Featherwing OLED
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5

// Attach the AmpDisplay to the display
AmpDisplay AmpDisp(display_p);

// U8G2 log is used at startup
//uint8_t u8log_buffer[LOG_WIDTH * LOG_HEIGHT];
//U8G2LOG displog;

void DisplaySetup() {
  display.begin();
  display.setFontMode(0);
  AmpDisp.displayMessage("START");

  //display.drawXBM(0, 0, 32, 32, icons_binary);
  
  // Start the startup log display
  //displog.begin(display, LOG_WIDTH, LOG_HEIGHT, u8log_buffer);
  //displog.setLineHeightOffset(0);
  //displog.setRedrawMode(0);
  //display.setFont(LOG_FONT);
  //displog.println("Startup");
}

void scheduleDim() {
  brightTime = millis();
}

void checkDim() {
  uint32_t currentTime = millis();
  if ((currentTime - brightTime) > DIM_TIME) {
    AmpDisp.dim();
  }
}

void OnMiniDSPConnected() {
  //Serial.println("MiniDSP connected");
  //Serial.flush();
  //displog.println("MiniDSP connected.");
  AmpDisp.displayMessage("CONNECTED");
}

void OnVolumeChange(uint8_t volume) {
  AmpDisp.volume(-volume/2.0);
  scheduleDim();
  //Serial.println("Volume: " + String(volume));
  //Serial.flush();
  //displog.println("Volume " + String(volume));
}

void OnMutedChange(bool isMuted) {
  //Serial.println("Muted status: " + String(isMuted ? "muted" : "unmuted"));
  //Serial.flush();
  //displog.println(String(isMuted ? "Muted" : "Unmuted"));
  //if (isMuted) displayMessage("MUTE"); else displayMessage("UNMUTE");
  AmpDisp.mute(isMuted);
  scheduleDim();
}

void OnSourceChange(uint8_t source) {
  AmpDisp.source((source_t) source);
  scheduleDim();
}

void OnParse(uint8_t * buf) {
  char bufStr[25];
  snprintf(bufStr, 25, "%02X %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
  AmpDisp.displayMessage(bufStr);
}

void OnNewLevels(float * levels ) {
  // For testing only - just display the two values, borrowing the source field
  char bufStr[25];
  snprintf(bufStr, 25, "L %6.1f,  R %6.1f", levels[0], levels[1]);
  AmpDisp.displayMessage(bufStr);
  scheduleDim();
}


void ledSetup() {
  pinMode(LED_BLUE, OUTPUT);
}

void setup() {
  DisplaySetup();
  ledSetup();
  //Serial.begin(115200);
  //while(!Serial) delay(10);
  if(thisUSB.Init() == -1) {
    //Serial.print(F("\r\nOSC did not start"));
    //displog.println("OSC did not start");
    while(1); // Halt
  }
  //Serial.println(F("\r\nMiniDSP 2x4HD Library Started"));
  //Serial.flush();
  //displog.println(F("MiniDSP 2x4HD Library Started"));

  // Register callbacks.
  ourMiniDSP.attachOnInit(&OnMiniDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&OnVolumeChange);
  ourMiniDSP.attachOnMutedChange(&OnMutedChange);
  ourMiniDSP.attachOnSourceChange(&OnSourceChange);
  ourMiniDSP.attachOnParse(&OnParse);
  //ourMiniDSP.attachOnNewLevels(&OnNewLevels);

  pinMode(5, INPUT_PULLUP); // Button C
  pinMode(6, INPUT_PULLUP); // Button B
  //Button A is pin 9 - used for UHS interrupt at the moment

}

bool buttonCPrev = false;
uint32_t lastTime;
//#define INTERVAL 600          // Interval between signal level requests. If shorter, automatic status updates may be missed.
bool MiniDSPConnected = false;

//void (* resetFunc) (void) = 0;

void loop() {

  ledOn(LED_BLUE);

  thisUSB.Task();

  ledOff(LED_BLUE);

  if (!ourMiniDSP.connected()) return;

  // Upon initial connection, the MiniDSP class issues a status request. When first connecting, allow time for response before issuing further requests.
  if (!MiniDSPConnected) {
    //ourMiniDSP.RequestStatus();
    MiniDSPConnected = true;
    lastTime = millis();    // Schedule the periodic request for later
    return;
  } 

  // Button C - status request
  bool buttonC = !digitalRead(5);
  if (buttonC && !buttonCPrev) {
    delay(100); // Simple debounce - needs to stay down for 100 ms
    if (!digitalRead(5)) ourMiniDSP.RequestStatus();
  }
  buttonCPrev = buttonC;

  #ifdef INTERVAL
  uint32_t currentTime = millis();
  if ( (currentTime - lastTime) > INTERVAL ) {
    ourMiniDSP.RequestLevels();
    lastTime = currentTime;
  }
  #endif

  checkDim();
  
  delay(50);
}
