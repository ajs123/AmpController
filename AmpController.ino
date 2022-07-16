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

// Interval (ms) between queries to the dsp.
// The update rate is mostly limited by display updates, which take 36 ms for the nrf52840 and generic OLED display.
constexpr uint32_t INTERVAL = 50;   

// Persistent state
uint32_t lastTime;    // millis() of the last timed query

// Enable OTA updates - to update without opening the box!
BLEDfu bledfu;
BLEDis bledis;

void BLESetup() {
  Bluefruit.begin();
  Bluefruit.setName("LXMini");

  bledfu.begin();

  bledis.setManufacturer("BistroDad");  // Not sure that we need dis if using only dfu
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
  AmpDisp.scheduleDim();
  AmpDisp.refresh();
  AmpDisp.setImmediateUpdate(false);  // Allows multiple sources to write to the display with control over refresh
}

// This needs to move to the input sensing handler
void analogSetup() {
  analogReadResolution(12);
  analogReference(AR_DEFAULT);  // 3.6 V
}

// Callbacks - MiniDSP

void OnMiniDSPConnected() {
  AmpDisp.displayMessage("CONNECTED");
  lastTime = millis();    // Delay the next timed request
  ourMiniDSP.RequestStatus();
  //needStatus = true;
}

// For debugging: Puts the first 8 bytes of received messages on the display.
void OnParse(uint8_t * buf) {
  char bufStr[25];
  snprintf(bufStr, 25, "%02X %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
  AmpDisp.displayMessage(bufStr);
}

// float dBSum(float dB1, float dB2) {
//   return 10 * log10(pow(dB1/10, 10) + pow(dB2/10, 10));
// }

// // VUMeter should move to AmpDisplay. Then a callback from MiniDSP can call that.
// void outputVUMeter(float * levels) {
//   //int leftSum = (int) dbSum(levels[0], levels[1]);
//   //int rightSum = (int) dbSum(levels[2], levels[3]);
//   int leftSum = ((int) levels[0] + (int) levels[1]) / 2;    // On L and R, sum the woofer and tweeter levels
//   int rightSum = ((int) levels[2] + (int) levels[3]) / 2;
//   uint8_t left = max( leftSum - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
//   uint8_t right = max( rightSum - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
//   AmpDisp.displayLRBarGraph(left, right, messageArea);
// }

constexpr float VUCoeff = 0.32 * INTERVAL / 50; // Approximates std. VU step response 90% at 300 ms
constexpr float signalFloorDB = -128.0;

class filteredVal {
  private:
    float currentValue;
    float coeff;
  public:
    filteredVal(float FIRcoeff, float signalFloorDB) : currentValue(signalFloorDB), coeff(FIRcoeff) {}
    float filter(float newValue) {
      return currentValue += (newValue - currentValue) * coeff;
    }
};

filteredVal leftLevel(VUCoeff, signalFloorDB);
filteredVal rightLevel(VUCoeff, signalFloorDB);

// NOTE: This will need to take the A-D difference into account.
// Perhaps it will get built into the getVolume and setVolume family.
/**
 * @brief Provides a VU meter based on input levels and the volume setting.
 * 
 * @param levels The two inputs, in dB.
 */
void inputVUMeter(float * levels) {
  float leftLevel_1 = levels[0] + ourMiniDSP.getVolumeDB();
  float rightLevel_1 = levels[1] + ourMiniDSP.getVolumeDB();
  int intLeftLevel = round(leftLevel.filter(leftLevel_1));
  int intRightLevel = round(rightLevel.filter(rightLevel_1));
  uint8_t left = !ourMiniDSP.isMuted() ? max( intLeftLevel - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL) : 0;
  uint8_t right = !ourMiniDSP.isMuted() ? max( intRightLevel - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL) : 0;

  AmpDisp.displayLRBarGraph(left, right, messageArea);
}

// Callbacks for response to both the remote and knob.
bool needStatus = false;    // This will probably go away, since reliance on responses to set commands seems to be reliable.

void volPlus() {
  //AmpDisp.displayMessage("VOL +");
  uint8_t currentVolume = static_cast<uint8_t>(ourMiniDSP.getVolume());
  if (currentVolume > ampOptions.maxVolume) ourMiniDSP.setVolume(--currentVolume);
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  AmpDisp.wakeup();   // Only really needed if already at maximum
  lastTime = millis();
  //needStatus = true;
}

void volMinus() {
  //AmpDisp.displayMessage("VOL -");
  uint8_t currentVolume = static_cast<uint8_t>(ourMiniDSP.getVolume());
  if (currentVolume != 0xFF) ourMiniDSP.setVolume(++currentVolume);
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  lastTime = millis();
  //needStatus = true;
}

void mute() {
  //AmpDisp.displayMessage("MUTE");
  ourMiniDSP.setMute(!ourMiniDSP.isMuted());
  lastTime = millis();
  //needStatus = true;
}

void input() {
  //AmpDisp.displayMessage("INPUT");
  ourMiniDSP.setSource(ourMiniDSP.getSource() ? 0 : 1);
  lastTime = millis();
  //needStatus = true;
}

// Hold the action button at startup to enter the setup menu
void menuCheck() {
  goButton.switchClosed();      // Returning a validated "closed" requires two samples
  delay(200);
  if (!goButton.switchClosed()) return;

  AmpDisp.displayMessage("Settings...", sourceArea);
  while (goButton.switchClosed()) delay(100);
  AmpDisp.displayMessage("", sourceArea);

  display.setFontPosBottom();   // The ArduinoMenu library expects this.
  OptionsMenu::menu(&display);
}

void optionsSetup() {
  // These could possibly be done in the private constructor, as long as 
  // dependencies such as (when debugging) Serial are initialized first.
  ampOptions.begin();
  ampOptions.load();
  ourRemote.loadFromOptions();
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

// The main state machine - uses a classic state pattern.
// Each state has a method invoked upon entry, 
// a poll method invoked each time around the loop,
// a request method invoked on each tick, and
// a callback for each possible event
// Naming convention for callbacks is on<source><event>(...)

class AmpState {
  public:
    virtual void onEntry(){}
    virtual void polls(){}
    virtual void requests(){}

    virtual void onDSPConnected(){}
    virtual void onDSPVolume(uint8_t volume){}
    virtual void onDSPMute(bool mute){}
    virtual void onDSPSource(uint8_t source){}
    virtual void onDSPInputLevels(float * levels){}
    virtual void onButtonShortPress(){}
    virtual void onButtonLongPressPending(){}
    virtual void onButtonLongPress(){}
    virtual void onButtonFullHold(){}
    virtual void onRemoteVolPlus(){}
    virtual void onRemoteVolMinus(){}
    virtual void onRemoteMute(){}
    virtual void onRemoteSource(){}
    virtual void onRemotePower(){}
    virtual void onKnobTurned(){}
    virtual void onTriggerOn(){}
    virtual void onTriggerOff(){}
};

void transitionTo(AmpState * newState);

// The ON state: respond to the remote, knob, button, and triggers, and maintain the display
class AmpOnState : public AmpState {
  void onEntry() override {
    ourMiniDSP.RequestStatus();
    lastTime = millis();
  }
  void polls() override {
    thisUSB.Task();
    ourRemote.Task();
    goButton.Task();
    //showUSBTaskState();
    AmpDisp.checkDim();
    AmpDisp.refresh();
  };

  void requests() override { ourMiniDSP.RequestInputLevels(); }

  void onDSPVolume(uint8_t volume) override { AmpDisp.volume(-volume/2.0); }
  void onDSPMute(bool isMuted) { AmpDisp.mute(isMuted); }
  void onDSPSource(uint8_t source) { AmpDisp.source((source_t) source); }
  void onDSPInputLevels(float * levels) { inputVUMeter(levels); }

  void onButtonShortPress() override { mute(); }
  void onButtonLongPressPending() override { AmpDisp.cueLongPress(); }
  void onButtonLongPress() override { input(); }
  void onButtonFullHold() override;

  void onRemoteVolPlus() override { volPlus(); }
  void onRemoteVolMinus() override { volMinus(); }
  void onRemoteMute() override { mute(); }
  void onRemoteSource() override { input(); }
  void onRemotePower() override;

  //void onKnobTurned() override {}
} ampOnState;

class AmpOffState : public AmpState {
  void onEntry() override {
    display.clear();
    display.updateDisplay();
  }
  void polls() override {
    ourRemote.Task();
    goButton.Task();
  }
  void onRemotePower() override; 
  void onButtonShortPress() override; 
} ampOffState;

class AmpWaitDSPState : public AmpState {
  void polls() override {
    thisUSB.Task();
    showUSBTaskState();
    AmpDisp.refresh();
    // TEMPORARY - Until power control is implemented, the dsp may already be connected
    if (ourMiniDSP.connected()) onDSPConnected();
  }
  void onDSPConnected() override { transitionTo(&ampOnState); }
} ampWaitDSPState;

void AmpOffState::onRemotePower() { transitionTo(&ampWaitDSPState); }
void AmpOffState::onButtonShortPress() { transitionTo(&ampWaitDSPState); }
void AmpOnState::onButtonFullHold() { transitionTo(&ampOffState); }
void AmpOnState::onRemotePower() { transitionTo(&ampOffState); }

// The state pattern context

AmpState * ampState{&ampOffState}; 

void polls() { ampState->polls(); }
void requests() { ampState->requests(); }
void onDSPConnected() { ampState->onDSPConnected(); }
void onDSPVolume(uint8_t volume) { ampState->onDSPVolume(volume); }
void onDSPMute(bool mute) { ampState->onDSPMute(mute); }
void onDSPSource(uint8_t source) { ampState->onDSPSource(source); }
void onDSPInputLevels(float * levels) { ampState->onDSPInputLevels(levels); }
void onButtonShortPress() { ampState->onButtonShortPress(); }
void onButtonLongPressPending() { ampState->onButtonLongPressPending(); }
void onButtonLongPress() { ampState->onButtonLongPress(); }
void onButtonFullHold() { ampState->onButtonFullHold(); }
void onRemoteVolPlus() { ampState->onRemoteVolPlus(); }
void onRemoteVolMinus(){ ampState->onRemoteVolMinus(); }
void onRemoteMute() { ampState->onRemoteMute(); }
void onRemoteSource() { ampState->onRemoteSource(); }
void onRemotePower() { ampState->onRemotePower(); }
void onKnobTurned() { ampState->onKnobTurned(); }

void transitionTo(AmpState * newState) {
  ampState = newState;
  ampState->onEntry();
}

void setup() {
  Serial.begin(115200);
  //while(!Serial) delay(10);

  optionsSetup();
  DisplaySetup();
  BLESetup();

  if(thisUSB.Init() == -1) {
    AmpDisp.displayMessage("USB didn't start.");
    AmpDisp.refresh();
    while(1); // Halt
  }

  menuCheck();

  // Register callbacks.
  ourMiniDSP.attachOnInit(&onDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&onDSPVolume);
  ourMiniDSP.attachOnMutedChange(&onDSPMute);
  ourMiniDSP.attachOnSourceChange(&onDSPSource);
  //ourMiniDSP.attachOnParse(&OnParse);
  ourMiniDSP.attachOnNewInputLevels(&onDSPInputLevels);
  // Remote and knob callbacks have fixed names so they don't need to be registered.

  ourMiniDSP.callbackOnResponse();
  ourRemote.listen();
}

void loop() {
  polls();

  // Periodic requests
  if (INTERVAL && ourMiniDSP.connected()) {
    uint32_t currentTime = millis();
    if ((currentTime - lastTime) >= INTERVAL) {
      requests();
      // //showFrameInterval(currentTime, lastTime); // DEBUG: Show the update interval
      // if (needStatus) {
      //   //ourMiniDSP.RequestStatus(); // If parsing set responses, it may be sufficient to just skip a levels request
      //   needStatus = false;
      // } else {
      //      ourMiniDSP.RequestInputLevels();
      // }
      lastTime = currentTime;
    }
  }
}
