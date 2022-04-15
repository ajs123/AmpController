#include <Arduino.h>
#include <MiniDSP.h>
#include "AmpDisplay.h"
#include "PowerControl.h"
#include "Options.h"

#ifdef U8X8_HAVE_HW_I2C // For this hardware (nrf52840) should just be able to include Wire.h
#include <Wire.h>
#endif

//#include <SPI.h> // This doesn't seem to matter, at least not for the Adafruit nrf52840

// Hardware
USB thisUSB;                                                  // USB via Host Shield
MiniDSP ourMiniDSP(&thisUSB);                                 // MiniDSP on thisUSB
U8G2_SH1107_64X128_F_HW_I2C display(U8G2_R1, U8X8_PIN_NONE);  // Adafruit OLED Featherwing display on I2C bus

// Interface class instances
AmpDisplay AmpDisp(&display);                                 // Abstracts the display elements

// Display stuff
#define LOG_FONT u8g2_font_5x7_tr //u8g2_font_7x14_tf
#define LOG_WIDTH 25 
#define LOG_HEIGHT 9

// Interval (ms) between queries to the dsp.
// The update rate is mostly limited by display updates.
// 
constexpr uint32_t INTERVAL = 30;   

// Persistent state
uint32_t lastTime;
uint32_t brightTime;

// Buttons on the Featherwing OLED
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5

// U8G2 log can be used at startup
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
  if (AmpDisp.dimmed()) return;
  uint32_t currentTime = millis();
  if ((currentTime - brightTime) > DIM_TIME) {
    AmpDisp.dim();
  }
}

void analogSetup() {
  analogReadResolution(12);
  analogReference(AR_DEFAULT);  // 3.6 V
}

void OnMiniDSPConnected() {
  //Serial.println("MiniDSP connected");
  //Serial.flush();
  //displog.println("MiniDSP connected.");
  AmpDisp.displayMessage("CONNECTED");
  lastTime = millis() + INTERVAL;
}

void OnVolumeChange(uint8_t volume) {
  if (volume < MAXIMUM_VOLUME) {
    ourMiniDSP.setVolume((uint8_t) MAXIMUM_VOLUME); 
    AmpDisp.volume(-MAXIMUM_VOLUME/2.0);
  } else {  
    AmpDisp.volume(-volume/2.0);
  }
    scheduleDim();
}

void OnMutedChange(bool isMuted) {
  AmpDisp.mute(isMuted);
  scheduleDim();
}

void OnSourceChange(uint8_t source) {
  AmpDisp.source((source_t) source);
  scheduleDim();
}

// For debugging: Puts the first 8 bytes of received messages on the display.
void OnParse(uint8_t * buf) {
  char bufStr[25];
  snprintf(bufStr, 25, "%02X %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
  AmpDisp.displayMessage(bufStr);
}

void OnNewLevels(float * levels ) {
  int leftSum = ((int) levels[0] + (int) levels[1]) / 2;    // On L and R, sum the woofer and tweeter levels
  int rightSum = ((int) levels[2] + (int) levels[3]) / 2;
  uint8_t left = max( leftSum - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
  uint8_t right = max( rightSum - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
  AmpDisp.displayLRBarGraph(left, right, messageArea);
}

// Prep the blue LED for use in debugging
void ledSetup() {
  pinMode(LED_BLUE, OUTPUT);
}

void setup() {
  DisplaySetup();
  ledSetup();
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

  // Register callbacks.
  ourMiniDSP.attachOnInit(&OnMiniDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&OnVolumeChange);
  ourMiniDSP.attachOnMutedChange(&OnMutedChange);
  ourMiniDSP.attachOnSourceChange(&OnSourceChange);
  //ourMiniDSP.attachOnParse(&OnParse);
  ourMiniDSP.attachOnNewLevels(&OnNewLevels);

  pinMode(BUTTON_A, INPUT_PULLUP); 
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

}

bool buttonCPrev = false;     // For button debounce

// DEBUG: for USB state reporting
void showTaskState() {
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

bool levelsLast;

void loop() {

  //ledOn(LED_BLUE);
  thisUSB.Task();
  //ledOff(LED_BLUE);

  showTaskState();      // Report the USB state in the message area. Useful in testing. May be useful in production with good messages.

  //if (taskState == 0xa0) thisUSB.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE); // If error, try reset
  //if (vbusState == 0x00) thisUSB.Init();

  // Periodic requests, such as input signal levels
  if (INTERVAL) {

    uint32_t currentTime = millis();
    if ((currentTime - lastTime) >= INTERVAL) {
      //showFrameInterval(currentTime, lastTime); // DEBUG: Show the update interval in

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

  // Button C - manual status request, for testing only
  bool buttonC = !digitalRead(5);
  if (buttonC && !buttonCPrev) {
    delay(100); // Simple debounce - needs to stay down for 100 ms
    if (!digitalRead(5)) ourMiniDSP.RequestStatus();
  }
  buttonCPrev = buttonC;

  checkDim();  // Dimming timer
}
