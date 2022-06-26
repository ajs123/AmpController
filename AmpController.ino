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
#include <math.h>

#include "Options.h"
#include "Configuration.h"

// Options store
Options & ampOptions = Options::instance();                   // Options store - singleton form

// Hardware and interface class instances
USB thisUSB;                                                  // USB via Host Shield
MiniDSP ourMiniDSP(&thisUSB);                                 // MiniDSP on thisUSB

U8G2_SH1107_64X128_F_HW_I2C display(U8G2_R1, U8X8_PIN_NONE);  // Adafruit OLED Featherwing display on I2C bus
AmpDisplay AmpDisp(&display);                                 // Live display on the OLED

// Input devices
//Remote ourRemote(IR_PIN);                                     // IR receiver
Remote & ourRemote = Remote::instance();                      // IR receiver - singleton form
Button goButton(ENCODER_BUTTON);                              // Encoder action button


// U8G2 log can be used at startup
//#define LOG_FONT u8g2_font_5x7_tr //u8g2_font_7x14_tf
//#define LOG_WIDTH 25 
//#define LOG_HEIGHT 9
//uint8_t u8log_buffer[LOG_WIDTH * LOG_HEIGHT];
//U8G2LOG displog;

// Interval (ms) between queries to the dsp.
// The update rate is mostly limited by display updates, which happen in callbacks.
constexpr uint32_t INTERVAL = 30;   

// Persistent state
uint32_t lastTime;

// Enable OTA updates - to update without opening the box!
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

// Checks on max volume shouldn't be needed here anymore
void OnVolumeChange(uint8_t volume) {
  //if (volume < options.maxVolume);
  if (volume < ampOptions.maxVolume) {
    ourMiniDSP.setVolume((uint8_t) ampOptions.maxVolume); 
    AmpDisp.volume(-ampOptions.maxVolume/2.0);
  } else {  
    AmpDisp.volume(-volume/2.0);
  }
}

void OnMutedChange(bool isMuted) {
  AmpDisp.mute(isMuted);
}

void OnSourceChange(uint8_t source) {
  AmpDisp.source((source_t) source);
}

// For debugging: Puts the first 8 bytes of received messages on the display.
void OnParse(uint8_t * buf) {
  char bufStr[25];
  snprintf(bufStr, 25, "%02X %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
  AmpDisp.displayMessage(bufStr);
}

float dBSum(float dB1, float dB2) {
  return 10 * log10(pow(dB1/10, 10) + pow(dB2/10, 10));
}

// VUMeter should move to AmpDisplay. Then a callback from MiniDSP can call that.
void VUMeter(float * levels) {
  //int leftSum = (int) dbSum(levels[0], levels[1]);
  //int rightSum = (int) dbSum(levels[2], levels[3]);
  int leftSum = ((int) levels[0] + (int) levels[1]) / 2;    // On L and R, sum the woofer and tweeter levels
  int rightSum = ((int) levels[2] + (int) levels[3]) / 2;
  uint8_t left = max( leftSum - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
  uint8_t right = max( rightSum - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
  AmpDisp.displayLRBarGraph(left, right, messageArea);
}

void takeInputLevels(float * levels) {
  char buf[20];
  //snprintf(buf, 20, "Input %f", levels[0] + levels[1]);
  //AmpDisp.displayMessage(buf);
}

// Callbacks for response to both the remote and knob.
bool needStatus = true;

void volPlus() {
  AmpDisp.displayMessage("VOL +");
  uint8_t currentVolume = ourMiniDSP.getVolume();
  uint8_t newVolume = currentVolume <= ampOptions.maxVolume ? ampOptions.maxVolume : currentVolume - 1;
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  ourMiniDSP.setVolume(newVolume);
  char buf[24];
  snprintf(buf, 24, "Vol %d %d -> %d", ampOptions.maxVolume, currentVolume, newVolume);
  AmpDisp.displayMessage(buf);
  AmpDisp.wakeup();

  needStatus = true;
}

// No need to set the volume if already at min (0xFF)
void volMinus() {
  AmpDisp.displayMessage("VOL -");
  uint8_t currentVolume = ourMiniDSP.getVolume();
  uint8_t newVolume = currentVolume == 0xFF ? 0xFF : currentVolume + 1;
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  ourMiniDSP.setVolume(newVolume);

  needStatus = true;
}

void mute() {
  AmpDisp.displayMessage("MUTE");
  ourMiniDSP.setMute(!ourMiniDSP.isMuted());

  needStatus = true;
}

void input() {
  AmpDisp.displayMessage("INPUT");
  ourMiniDSP.setSource(ourMiniDSP.getSource() ? 0 : 1);

  needStatus = true;
}

void power() {
  AmpDisp.displayMessage("POWER");
}

// Action button callbacks.
// Some of these might go away when no longer needed for debugging, 
// as they just display a message and invoke the actual action callback.

void shortPress() {
  AmpDisp.displayMessage("SHORT PRESS");
  mute();
}

void longPressPending() {
  AmpDisp.displayMessage("LONG PRESS PENDING");
  AmpDisp.cueLongPress();
}

void longPress() {
  AmpDisp.displayMessage("LONG PRESS");
  input();
}

// Kludge just to test full hold --> setup
bool menuState = false;
void fullHold() {
  AmpDisp.displayMessage("FULL HOLD");
  AmpDisp.cancelLongPress();
  menuState = true;
}

// Prep the blue LED for use in debugging
//void ledSetup() {
//  pinMode(LED_BLUE, OUTPUT);
//}

// Hold the action button at startup to enter the setup menu
void menuCheck() {
  goButton.switchClosed();      // Returning a validated "closed" requires two samples
  delay(200);
  if (!goButton.switchClosed()) return;

  AmpDisp.displayMessage("Settings...", sourceArea);
  while (goButton.switchClosed()) delay(100);
  AmpDisp.displayMessage("", sourceArea);

  display.setFontPosBottom();
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

  DisplaySetup();
  //ledSetup();

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
  }

  menuCheck();

  // Register callbacks.
  ourMiniDSP.attachOnInit(&OnMiniDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&OnVolumeChange);
  ourMiniDSP.attachOnMutedChange(&OnMutedChange);
  ourMiniDSP.attachOnSourceChange(&OnSourceChange);
  //ourMiniDSP.attachOnParse(&OnParse);
  ourMiniDSP.attachOnNewLevels(&VUMeter);
  ourMiniDSP.attachOnNewInputLevels(&takeInputLevels);

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

  // If we're going to provide menu access without a power cycle, this ought to include
  // navigation commands or equivalent manipulation of the nav object to bring us
  // to the top of the menu
  if (menuState) {
    menuCheck();
    menuState = false; // This would be replaced with the top-level state
  }

/* display test
  AmpDisp.source(Analog);
  AmpDisp.volume(0.0);
  delay(2000);
  AmpDisp.mute(true);
  delay(2000);
  AmpDisp.mute(false);
  AmpDisp.volume(-120);
  delay(2000);
  AmpDisp.wakeup();
  AmpDisp.dim();
  delay(5000);
*/
  // Periodic requests, such as input signal levels
  if (INTERVAL && ourMiniDSP.connected()) {

    uint32_t currentTime = millis();

    if ((currentTime - lastTime) >= INTERVAL) {
      //showFrameInterval(currentTime, lastTime); // DEBUG: Show the update interval in

      static bool levelsLast;
      if (levelsLast) {
        if (needStatus) {
          ourMiniDSP.RequestStatus();
          needStatus = false;
        }
      }
      else {
        ourMiniDSP.RequestLevels();   // Inputs and outputs
        //ourMiniDSP.RequestOutputLevels();   // Just outputs
      }
      levelsLast = !levelsLast;
      lastTime = currentTime;
    }
  }

  AmpDisp.checkDim(); 

}
