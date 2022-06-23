#include <Arduino.h>
#include <MiniDSP.h>
#include "AmpDisplay.h"
#include "PowerControl.h"
#include "RemoteHandler.h"
#include "Button.h"
#include "OptionsMenu.h"
#include <Wire.h>   // For the I2C display. Also provides Serial.
#include <SPI.h>    // This doesn't seem to matter, at least not for the Adafruit nrf52840
#include <bluefruit.h>  // Experimental - for OTA updates

#include "Options.h"
#include "Configuration.h"

// Options store
Options & ampOptions = Options::instance();

// Hardware and interface class instances
USB thisUSB;                                                  // USB via Host Shield
MiniDSP ourMiniDSP(&thisUSB);                                 // MiniDSP on thisUSB

U8G2_SH1107_64X128_F_HW_I2C display(U8G2_R1, U8X8_PIN_NONE);  // Adafruit OLED Featherwing display on I2C bus
AmpDisplay AmpDisp(&display);                                 // Live display on the OLED

//Remote ourRemote(IR_PIN);                                     // IR receiver
Remote & ourRemote = Remote::instance();
Button goButton(ENCODER_BUTTON);                              // Encoder action button

// For the display log, if used
#define LOG_FONT u8g2_font_5x7_tr //u8g2_font_7x14_tf
#define LOG_WIDTH 25 
#define LOG_HEIGHT 9

// Interval (ms) between queries to the dsp.
// The update rate is mostly limited by display updates, which happen in callbacks.
constexpr uint32_t INTERVAL = 30;   

// Persistent state
uint32_t lastTime;

// U8G2 log can be used at startup
//uint8_t u8log_buffer[LOG_WIDTH * LOG_HEIGHT];
//U8G2LOG displog;

// Experimental - enable OTA updates
BLEDfu bledfu;
BLEDis bledis;

void BLESetup() {
  Bluefruit.begin();
  Bluefruit.setName("LXMini");

  bledfu.begin();

  bledis.setManufacturer("BistroDad");
  bledis.setModel("LXMini Amp");
  bledis.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(bledfu);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(300);
}

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

// This needs to move to the input sensing handler
void analogSetup() {
  analogReadResolution(12);
  analogReference(AR_DEFAULT);  // 3.6 V
}

// Callbacks - MiniDSP

void OnMiniDSPConnected() {
  AmpDisp.displayMessage("CONNECTED");
  lastTime = millis() + INTERVAL;
}

void OnVolumeChange(uint8_t volume) {
  //if (volume < options.maxVolume);
  if (volume < MAXIMUM_VOLUME) {
    ourMiniDSP.setVolume((uint8_t) MAXIMUM_VOLUME); 
    AmpDisp.volume(-MAXIMUM_VOLUME/2.0);
  } else {  
    AmpDisp.volume(-volume/2.0);
  }
//    AmpDisp.scheduleDim();
}

void OnMutedChange(bool isMuted) {
  AmpDisp.mute(isMuted);
//  AmpDisp.scheduleDim();
}

void OnSourceChange(uint8_t source) {
  AmpDisp.source((source_t) source);
//  AmpDisp.scheduleDim();
}

// For debugging: Puts the first 8 bytes of received messages on the display.
void OnParse(uint8_t * buf) {
  char bufStr[25];
  snprintf(bufStr, 25, "%02X %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
  AmpDisp.displayMessage(bufStr);
}

// VUMeter should move to AmpDisplay. Then a callback from MiniDSP can call that.
void VUMeter(float * levels ) {
  int leftSum = ((int) levels[0] + (int) levels[1]) / 2;    // On L and R, sum the woofer and tweeter levels
  int rightSum = ((int) levels[2] + (int) levels[3]) / 2;
  uint8_t left = max( leftSum - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
  uint8_t right = max( rightSum - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
  AmpDisp.displayLRBarGraph(left, right, messageArea);
}

// Callbacks - remote and knob.

void volPlus() {
  AmpDisp.displayMessage("VOL +");
  uint8_t currentVolume = ourMiniDSP.getVolume();
  uint8_t newVolume = currentVolume <= MAXIMUM_VOLUME ? MAXIMUM_VOLUME : currentVolume - 1;
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  ourMiniDSP.setVolume(newVolume);
  AmpDisp.wakeup();
}

void volMinus() {
  AmpDisp.displayMessage("VOL -");
  uint8_t currentVolume = ourMiniDSP.getVolume();
  uint8_t newVolume = currentVolume >= 0xFE ? 0xFE : currentVolume + 1;
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  ourMiniDSP.setVolume(newVolume);
}

void mute() {
  AmpDisp.displayMessage("MUTE");
  ourMiniDSP.setMute(!ourMiniDSP.isMuted());
}

void input() {
  AmpDisp.displayMessage("INPUT");
  ourMiniDSP.setSource(ourMiniDSP.getSource() ? 0 : 1);
}

void power() {
  AmpDisp.displayMessage("POWER");
}

// Action button callbacks.
// Some of these might go away when no longer needed for debugging, as they just display a message and invoke the actual action callback.
void shortPress() {
  AmpDisp.displayMessage("SHORT PRESS");
  mute();
}

// Cue the user re a long press, which changes the source.
void longPressPending() {
  AmpDisp.displayMessage("LONG PRESS PENDING");
  AmpDisp.cueLongPress();
}

void longPress() {
  AmpDisp.displayMessage("LONG PRESS");
  input();
}

void fullHold() {
  AmpDisp.displayMessage("FULL HOLD");
  AmpDisp.cancelLongPress();
  //OptionsMenu::menu(&display);
}

// Prep the blue LED for use in debugging
void ledSetup() {
  pinMode(LED_BLUE, OUTPUT);
}

void menuCheck() {
  goButton.switchClosed();      // Returning a validated "closed" requires two samples
  delay(200);
  if (!goButton.switchClosed()) return;

  AmpDisp.displayMessage("Settings...", sourceArea);
  while (goButton.switchClosed()) delay(100);
  AmpDisp.displayMessage("", sourceArea);

  OptionsMenu::menu(&display);
}

void setup() {
  Serial.begin(115200);
  //while(!Serial) delay(10);

  // These could probably be done in the private constructor, as long as 
  // dependencies such as (when debugging) Serial are initialized first.
  ampOptions.begin();
  ampOptions.load();
  ourRemote.loadFromOptions();
  //options.save(); // For testing. No normal reason to do this here.

  DisplaySetup();
  ledSetup();

  BLESetup();

  #if (ENABLE_UHS_DEBUGGING == 1)  // From settings.h in the UHS library
    Serial.begin(115200);
    while(!Serial) delay(10);
    Serial.print("STARTUP\r\n");
  #endif
  if(thisUSB.Init() == -1) {
    #if (ENABLE_UHS_DEBUGGING == 1)
      //Serial.print(F("\r\nOSC did not start"));
    #endif
    //displog.println("OSC did not start");
    AmpDisp.displayMessage("USB did not start");
    while(1); // Halt

  //lastTime = millis() + INTERVAL;
  }

  menuCheck();

  // Register callbacks.
  ourMiniDSP.attachOnInit(&OnMiniDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&OnVolumeChange);
  ourMiniDSP.attachOnMutedChange(&OnMutedChange);
  ourMiniDSP.attachOnSourceChange(&OnSourceChange);
  //ourMiniDSP.attachOnParse(&OnParse);
  ourMiniDSP.attachOnNewLevels(&VUMeter);

  // Remote and knob callbacks have fixed names so they don't need to be registered.

  ourRemote.listen();
}

// DEBUG: for USB state reporting
void showUSBTaskState() {
  static uint16_t lastUSBState = 0xFFFF;
  char strBuf[20];
  uint8_t taskState = thisUSB.getUsbTaskState();
  uint8_t vbusState = thisUSB.getVbusState();
  uint16_t USBState = taskState | (vbusState << 8);
  if (USBState != lastUSBState) {
    snprintf(strBuf, sizeof(strBuf), "TASK %02x   VBUS %02x", taskState, vbusState);
    AmpDisp.displayMessage(strBuf);
    lastUSBState = USBState;
  }
}

// DEBUG: for showing the update interval
void showFrameInterval(uint32_t currentTime, uint32_t lastTime) {
  constexpr uint32_t frameReportInterval = 1000;
  char strBuf[24];
  static uint16_t intervalAccum = 0;
  static uint16_t intervalCount = 0;
  static uint32_t lastFrameReport = millis();

  intervalAccum += currentTime - lastTime;
  intervalCount ++;

  if ((currentTime - lastFrameReport) >= frameReportInterval) {
    snprintf(strBuf, sizeof(strBuf), "Int %d", intervalAccum / intervalCount);
    AmpDisp.displayMessage(strBuf, sourceArea);
    intervalAccum = 0;
    intervalCount = 0;
    lastFrameReport = currentTime;
  }
}

void loop() {

  thisUSB.Task();
  ourRemote.Task();
  goButton.Task();

  showUSBTaskState();      // Useful in testing. May be useful in production with good messages.

  // Periodic requests, such as input signal levels
  if (INTERVAL) {

    uint32_t currentTime = millis();

    // Should be able to just do 
    //uint32_t sinceLast = currentTime - lastTime;
    //if (sinceLast >= INTERVAL) delay(sinceLast - INTERVAL);
    // ...or do we want top-of-loop tasks to happen more often?
    
    if ((currentTime - lastTime) >= INTERVAL) {
      //showFrameInterval(currentTime, lastTime); // DEBUG: Show the update interval in

      static bool levelsLast;
      if (levelsLast) {
        ourMiniDSP.RequestStatus();
      }
      else {
        ourMiniDSP.RequestOutputLevels();
      }
      levelsLast = !levelsLast;
      lastTime = currentTime;
    }
  }

  AmpDisp.checkDim(); 

}
