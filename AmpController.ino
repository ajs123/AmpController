#include <Arduino.h>
#include "src/UHS/MiniDSP.h"
//#include <MiniDSP.h>
#include "AmpDisplay.h"
#include "PowerControl.h"
#include "RemoteHandler.h"
#include "Button.h"
#include "Knob.h"
#include <RotaryEncoder.h>
#include "OptionsMenu.h"
#include <Wire.h>       // For the I2C display. Also provides Serial.
#include <SPI.h>        // This doesn't seem to matter, at least not for the Adafruit nrf52840
#include <bluefruit.h>  // For OTA updates
#include "Options.h"
#include "Configuration.h"
#include "logo.h"
#include "InputSensing.h"
//#include "NoisyLogo.h"

// Options store
Options & ampOptions = Options::instance();                   // Options store - singleton form

// Hardware and interface class instances
USB thisUSB;                                                  // USB via Host Shield
MiniDSP ourMiniDSP(&thisUSB);                                 // MiniDSP on thisUSB

U8G2_SH1107_64X128_F_HW_I2C display(U8G2_R1, U8X8_PIN_NONE);  // Adafruit OLED Featherwing display on I2C bus
AmpDisplay ampDisp(&display);                                 // Live display on the OLED

// Input devices
//Remote ourRemote(IR_PIN);                                     // IR receiver
Remote & ourRemote = Remote::instance();                      // IR receiver - singleton form
Button goButton(ENCODER_BUTTON);                              // Encoder action button
Knob knob(ENCODER_A, ENCODER_B);                              // Rotary encoder

// Amp control: power relay and amp enables
PowerControl powerControl;

// Input and trigger monitoring
InputMonitor inputMonitor(1);  // Arg is minutes. Will get set to the actual option value
TriggerSensing triggerMonitor;

// Interval (ms) between queries to the dsp.
// Limited mainly by the 36 ms for refresh of the nrf52840 and generic OLED display.
constexpr uint32_t INTERVAL = 50;   

// Persistent state
uint32_t lastTime;    // millis() of the last timed query

// BLE services
BLEDfu bledfu;      // Device firmware update
BLEDis bledis;      // Device information service

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
  //ampDisp.displayMessage("START");
  ampDisp.scheduleDim();
  ampDisp.refresh();
  ampDisp.setImmediateUpdate(false);
}

// For debugging: Puts the first 8 bytes of received messages on the display.
void OnParse(uint8_t * buf) {
  char bufStr[28];
  snprintf(bufStr, 28, "%02X %02X %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9]);
  ampDisp.displayMessage(bufStr);
}

constexpr float VUCoeff = 0.32 * INTERVAL / 50; // Approximates std. VU step response 90% at 300 ms
constexpr float signalFloorDB = -128.0;

IIR<float> leftLevel(VUCoeff, signalFloorDB);
IIR<float> rightLevel(VUCoeff, signalFloorDB);

float sourceGain(source_t source) {
  return (source == source_t::Analog) ? (float)ampOptions.analogDigitalDifference : 0.0;
}

void handleInputLevels(float * levels) {
  float left = leftLevel.next(levels[1]);
  float right = rightLevel.next(levels[1]);
  inputVUMeter(left, right);
  inputMonitor.task(left, right);
}

/**
 * @brief Provides a VU meter based on input levels and the volume setting.
 * 
 * @param levels The two inputs, in dB.
 */
void inputVUMeter(float leftLevel, float rightLevel) {
  int intLeftLevel = round(leftLevel + ourMiniDSP.getVolumeDB() + sourceGain(ourMiniDSP.getSource()));// + ourMiniDSP.getVolumeOffsetDB());
  int intRightLevel = round(rightLevel + ourMiniDSP.getVolumeDB() + sourceGain(ourMiniDSP.getSource()));// + ourMiniDSP.getVolumeOffsetDB());
  uint8_t left = !ourMiniDSP.isMuted() ? max( intLeftLevel - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL) : 0;
  uint8_t right = !ourMiniDSP.isMuted() ? max( intRightLevel - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL) : 0;
  ampDisp.displayLRBarGraph(left, right, messageArea);
}

// Utility functions for the various callbacks

template<class T> 
inline T limit(const T& value, const T& min, const T& max) { 
  T v = (value < min) ? min : value;
  return (v > max) ? max : v; 
  }

void setVolume(uint8_t volume) {
  ourMiniDSP.setVolume(limit(volume, ampOptions.maxVolume, uint8_t(0xFF)));   // Unsigned int representing negative dB, so min is maxVolume
  lastTime = millis();
}

void volChange(int8_t change) {
  int currentVolume = ourMiniDSP.getVolume();
  int newVolume = currentVolume - change;     // + change is - change in the MiniDSP setting
  newVolume = limit(newVolume, int(ampOptions.maxVolume), 0xFF); //min( max(newVolume, ampOptions.maxVolume), 0xFF);
  if (newVolume != currentVolume) ourMiniDSP.setVolume(static_cast<uint8_t>(newVolume));
  ampDisp.wakeup();
  lastTime = millis();
}

void volPlus() {
  uint8_t currentVolume = static_cast<uint8_t>(ourMiniDSP.getVolume());
  if (currentVolume > ampOptions.maxVolume) ourMiniDSP.setVolume(--currentVolume);
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  ampDisp.wakeup();   // Only really needed if already at maximum
  lastTime = millis();
}

void volMinus() {
  uint8_t currentVolume = static_cast<uint8_t>(ourMiniDSP.getVolume());
  if (currentVolume != 0xFF) ourMiniDSP.setVolume(++currentVolume);
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  lastTime = millis();
}

void setMute(bool muted) {
  ourMiniDSP.setMute(muted);
  lastTime = millis();
}

void mute() {
  ourMiniDSP.setMute(!ourMiniDSP.isMuted());
  lastTime = millis();
  //static bool m {false};
  //m = !m;
  //if (m) powerControl.ampDisable(); else powerControl.ampEnable();
}

void setSource(source_t source) {
  ourMiniDSP.setSource(source);
  //ourMiniDSP.setVolumeOffset(source == source_t::Analog ? 0 : ampOptions.analogDigitalDifference);
  lastTime = millis();
}

source_t flipSource() {
  //ampDisp.displayMessage("INPUT");
  source_t newSource = ourMiniDSP.getSource() == source_t::Analog ? source_t::Toslink : source_t::Analog;
  //setSource(newSource);
  //lastTime = millis();
  return newSource;
}

void setInputGain(source_t source) {
  //const float aGains[] = {6.0, 6.0};
  //const float dGains[] = {0.0, 0.0};
  ourMiniDSP.setInputGain(sourceGain(source));
  lastTime = millis();
}

void optionsSetup() {
  // These could possibly be done in the private constructor, as long as 
  // dependencies such as (when debugging) Serial are initialized first.
  ampOptions.begin();
  ampOptions.load();
  ourRemote.loadFromOptions();
}

void showLogo() {
  display.clear();
  display.drawXBMP(0, (64 - logo_height) / 2, logo_width, logo_height, logo_bits);
  display.updateDisplay();
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
    ampDisp.displayMessage(strBuf);
    //if ((taskState != 0x90) || (vbusState != 0x02)) ampDisp.displayMessage("WAIT..."); 
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
    ampDisp.displayMessage(strBuf, sourceArea);
    intervalAccum = 0;
    intervalCount = 0;
    lastFrameReport = currentTime;
  }
}

// The main state machine - uses a classic state pattern.
// Each state has 
//   a method invoked upon entry, 
//   a poll method invoked each time around the loop,
//   a request method invoked on each tick, and
//   a callback for each possible event.
// Naming convention for callbacks is on<Source><Event>(...)

class AmpState {
  public:
    virtual void onEntry(){}
    virtual void polls(){}
    virtual void requests(){}

    virtual void onDSPConnected(){}
    virtual void onDSPVolume(uint8_t volume){}
    virtual void onDSPMute(bool mute){}
    virtual void onDSPSource(source_t source){}
    virtual void onDSPInputLevels(float * levels){}
    virtual void onDSPInputGains(float * gains) {}
    virtual void onButtonShortPress(){}
    virtual void onButtonLongPressPending(){}
    virtual void onButtonLongPress(){}
    virtual void onButtonFullHold(){}
    virtual void onKnobTurned(){}
    virtual void onRemoteVolPlus(){}
    virtual void onRemoteVolMinus(){}
    virtual void onRemoteMute(){}
    virtual void onRemoteSource(){}
    virtual void onRemotePower(){}
    virtual void onKnobTurned(int8_t change){}
    virtual void onTriggerRise(trigger_t source){}
    virtual void onTriggerFall(trigger_t source){}
    virtual void onSilence(){}
    virtual void onMenuExit(){}
};

void transitionTo(AmpState * newState);

// The Off state - just watch the remote, button, and triggers
class AmpOffState : public AmpState {
  void onEntry() override {
    powerControl.ampDisable();
    powerControl.powerOff();
    display.clear();
    display.updateDisplay();
    ourRemote.listen();
  }
  void polls() override {
    ourRemote.Task();
    goButton.Task();
    triggerMonitor.task();
  }
  void onButtonFullHold() override;                 // --> Menu - See the transition table
  void onButtonShortPress() override;               // --> Start seq - See transition table

  void onTriggerRise(trigger_t sources) override;   // --> Start seq - See transition table

  void onRemotePower() override;                    // -- Start seq - See transition table below
} ampOffState;

// Menu state
class AmpMenuState : public AmpState {
  void onEntry() override {
    ampDisp.displayMessage("Settings...", sourceArea);
    ampDisp.refresh();
    while (goButton.switchClosed()) delay(100);
    ampDisp.displayMessage("", sourceArea);
    display.setFontPosBottom();   // The ArduinoMenu library expects this.
    OptionsMenu::reset();
    OptionsMenu::begin();
  }

  void polls() {
    knob.task();
    goButton.Task();
    OptionsMenu::task();
  }
  
  void onKnobTurned(int8_t change) override {
    if (!change) return;
    if (change > 0) OptionsMenu::cursorUp(); else OptionsMenu::cursorDown(); 
  }

  void onButtonShortPress() override { OptionsMenu::enter(); }
  void onMenuExit() override;                       // --> Off - See the transition table
} ampMenuState;

// DSP wait state - power up and watch for the DSP to become connected
const uint32_t maxDSPStartupTime = 10000; // ms
class AmpWaitDSPState : public AmpState {
  uint32_t entryTime {0};
  void onEntry() override {
    entryTime = millis();
    powerControl.ampDisable();
    powerControl.powerOn();
    showLogo();
    ampDisp.displayMessage(".");
    ampDisp.refresh();
    ampDisp.wakeup();
  }
  void onDSPTimeout();                              // --> Cycle power - See transition table
  void polls() override {
    thisUSB.Task();
    if ((millis() - entryTime) > maxDSPStartupTime) onDSPTimeout();
    //showUSBTaskState();
    //ampDisp.refresh();
    // TEMPORARY: Until power control is implemented, the dsp may already be connected
    //if (ourMiniDSP.connected()) onDSPConnected();
  }
  void onDSPConnected() override;                   // --> Source check - See transition table
} ampWaitDSPState;

// DSP timeout state - power down for 2 sec and retry
const uint32_t DSPPowerDownTime = 2000; // ms
class AmpCyclePowerState : public AmpState {
  uint32_t entryTime {0};
  void onEntry() override {
    powerControl.powerOff();
    entryTime = millis();
  }
  void onTime();                                    // --> Wait for DSP - see transition table
  void polls() override {
    if ((millis() - entryTime) > DSPPowerDownTime) onTime();
  }
} ampCylcePowerState;

// Source wait state - as needed per the triggers, ensure that we're set to the correct input
// or if entered via the button or remote, just switch inputs regardless of the triggers
class AmpWaitSourceState : public AmpState {
  private:
    source_t desiredSource {source_t::Unset};
  public:
    void setDesiredSource(source_t source) { desiredSource = source; }

  void onEntry() override { 
    ampDisp.displayMessage(".."); 
    ampDisp.refresh();
    if (desiredSource != source_t::Unset) setSource(desiredSource);
    }
  void polls() override {
    thisUSB.Task();
  }
  void requests() override {
    ourMiniDSP.requestSource(); // Could be requestSource()
  }
  void toSetGain();
  void onDSPSource(source_t source) override {
    if (desiredSource != source_t::Unset) {
      //Serial.printf("Handling desired source %d and actual %d\n", (uint8_t)desiredSource, (uint8_t)source);
      if (source == desiredSource) {
        toSetGain();
        desiredSource = source_t::Unset;
      } else {
        setSource(desiredSource);
      }
      return;
    }
    // Check input against what's called for by the triggers
    trigger_t triggers = triggerMonitor.getTriggers();
    if (triggers.analog && triggers.digital) {              // If both, leave source as is
      toSetGain();
      return;
    }
    if (triggers.analog && (source != source_t::Analog)) {
      setSource(source_t::Analog);
      return;
    }
    if (triggers.digital && (source != source_t::Toslink)) {
      setSource(source_t::Toslink);
      return;
    }
    toSetGain();
  }
} ampWaitSourceState;

inline bool fEqual(float x, float y, float p = 0.1) {
  return (abs(x - y) < p);
}

// Set gain state - ensure that input gains match the options settings
class AmpSetGainState : public AmpState {
  void onEntry() override { 
    ampDisp.displayMessage("..G"); 
    ampDisp.refresh();
    }
  void polls() override {
    thisUSB.Task();
  }
  void requests() override {
    setInputGain(ourMiniDSP.getSource());
  }
  void toWaitVolume();
  void onDSPInputGains(float * gains) {
    //Serial.printf("Gains set to %f, %f\n", gains[0], gains[1]);
    float reqGain = sourceGain(ourMiniDSP.getSource());
    if (fEqual(gains[1], reqGain) && fEqual(gains[1], reqGain)) toWaitVolume();   
  }
} ampSetGainState;

// Volume wait state - ensure that the volume is within range (generally, and for startup)
class AmpWaitVolumeState : public AmpState {
  void onEntry() override { 
    ampDisp.displayMessage("..."); 
    ampDisp.refresh();
    }
  void polls() override {
    thisUSB.Task();
  }
  void requests() override {
    ourMiniDSP.requestVolume(); 
  }
  void toWaitMute();
  // WE DON'T NEED A MAX VOLUME CHECK WHEN ALREADY ON AND SWITCHING INPUTS
  void onDSPVolume(uint8_t volume) override {
    if (volume < ampOptions.maxInitialVolume) setVolume(ampOptions.maxInitialVolume);
    else toWaitMute();                              // --> Mute check - See transition table
  }
} ampWaitVolumeState;

// Mute wait state - ensure that the DSP is unmuted
class AmpWaitMuteState : public AmpState {
  void onEntry() override { 
    ampDisp.displayMessage("...."); 
    ampDisp.refresh();
    }
    //setMute(false); }
  void polls() override {
    thisUSB.Task();
  }
  void requests() override { 
    ourMiniDSP.requestMute(); // Could be requestMute();
  }
  void toOn();
  void onDSPMute(bool isMuted) override {
    if (isMuted) setMute(false);
    else toOn();                                    // --> On - See transition table
  }
} ampWaitMuteState;

// The ON state: respond to the remote, knob, button, and triggers, and maintain the display
class AmpOnState : public AmpState {
  void onEntry() override {
    RotaryEncoder.read();     // ensure that the change count is zero.
    powerControl.ampEnable();
    inputMonitor.setTimout(ampOptions.autoOffTime);
    inputMonitor.resetTimer();
    display.clear();
    ampDisp.source((source_t) ourMiniDSP.getSource());
    ampDisp.volume(-ourMiniDSP.getVolume()/2.0);
    ampDisp.mute(ourMiniDSP.isMuted());
    ourRemote.listen();
  }
  void polls() override {
    thisUSB.Task();
    ourRemote.Task();
    knob.task();
    goButton.Task();
    triggerMonitor.task();
    ampDisp.checkDim();
    ampDisp.refresh();
  };

  void requests() override { ourMiniDSP.RequestInputLevels(); }

  void onDSPVolume(uint8_t volume) override { ampDisp.volume(-volume/2.0); }
  void onDSPMute(bool isMuted) { ampDisp.mute(isMuted); }
  void onDSPSource(source_t source) { ampDisp.source((source_t) source); }
  void onDSPInputLevels(float * levels) { handleInputLevels(levels); }

  void toOff();
  void toSource();

  void onButtonShortPress() override { mute(); }
  void onButtonLongPressPending() override { ampDisp.cueLongPress(); }
  void onButtonLongPress() override { 
    ampWaitSourceState.setDesiredSource(flipSource());
    toSource();
  }
  void onButtonFullHold() override;                         // --> Off - See transition table

  void onRemoteVolPlus() override { volPlus(); }
  void onRemoteVolMinus() override { volMinus(); }
  void onRemoteMute() override { mute(); }
  void onRemoteSource() override { 
    ampWaitSourceState.setDesiredSource(flipSource());
    toSource();
  }
  void onRemotePower() override;                            // --> Off - See transition table

  void onKnobTurned(int8_t change) override { volChange(change); }

  void onTriggerFall(trigger_t triggers) override {
    // triggers indicates which has fallen
    source_t source = ourMiniDSP.getSource();
    if (triggers.analog && (source == source_t::Analog)) {
      if (!triggerMonitor.getTriggers().digital) {
        toOff();                                            // --> Off - see transition table
        return;
      }
      toSource();                                           // --> Switch source - see transition table
    }
    if (triggers.digital && (source == source_t::Toslink)) {
      if (!triggerMonitor.getTriggers().analog) {
        toOff();                                            // --> Off - see transition table
        return;
      }
      toSource();                                           // --> Switch source - see transition table
    }
  }

  void onSilence() override {
    if (ourMiniDSP.getSource() == source_t::Analog) {
      if (triggerMonitor.getTriggers().analog) return;  // Ignore silence when trigger present
      if (triggerMonitor.getTriggers().digital) {
        toSource();                                     // Silence with other trigger present
        return;
      }
      toOff();                                          // Silence with no triggers
      return;
    } else{
      if (triggerMonitor.getTriggers().digital) return;
      if (triggerMonitor.getTriggers().analog) {
        toSource();
        return;
      }
      toOff();
      return;
    }
  };                      

} ampOnState;

// State transitions
void AmpOffState::        onRemotePower()           { transitionTo(&ampWaitDSPState); }
void AmpOffState::        onButtonShortPress()      { transitionTo(&ampWaitDSPState); }
void AmpOffState::        onButtonFullHold()        { transitionTo(&ampMenuState);    }
void AmpOffState::        onTriggerRise(trigger_t)  { transitionTo(&ampWaitDSPState); }     
void AmpMenuState::       onMenuExit()              { transitionTo(&ampOffState);     }
void AmpWaitDSPState::    onDSPConnected()          { transitionTo(&ampWaitSourceState); }
void AmpWaitDSPState::    onDSPTimeout()            { transitionTo(&ampCylcePowerState); }
void AmpCyclePowerState:: onTime()                  { transitionTo(&ampWaitDSPState); }
void AmpWaitSourceState:: toSetGain()               { transitionTo(&ampSetGainState); }
void AmpSetGainState::    toWaitVolume()            { transitionTo(&ampWaitVolumeState); }  // when source verified to match triggers
void AmpWaitVolumeState:: toWaitMute()              { transitionTo(&ampWaitMuteState); }    // when volume verified to be within limits
void AmpWaitMuteState::   toOn()                    { transitionTo(&ampOnState); }          // when mute verified to be off
void AmpOnState::         onRemotePower()           { transitionTo(&ampOffState); }
void AmpOnState::         onButtonFullHold()        { transitionTo(&ampOffState); }
void AmpOnState::         toOff()                   { transitionTo(&ampOffState); }         // silence without trigger, or trigger loss when the other is low
void AmpOnState::         toSource()                { transitionTo(&ampWaitSourceState); }  // trigger loss when the other is high

// The state pattern context

AmpState * ampState {&ampOffState}; 

void polls() { ampState->polls(); }
void requests() { ampState->requests(); }
void onDSPConnected() { ampState->onDSPConnected(); }
void onDSPVolume(uint8_t volume) { ampState->onDSPVolume(volume); }
void onDSPMute(bool mute) { ampState->onDSPMute(mute); }
void onDSPSource(source_t source) { ampState->onDSPSource(source); }
void onDSPInputLevels(float * levels) { ampState->onDSPInputLevels(levels); }
void onDSPInputGains(float * gains) { ampState->onDSPInputGains(gains); }
void onButtonShortPress() { ampState->onButtonShortPress(); }
void onButtonLongPressPending() { ampState->onButtonLongPressPending(); }
void onButtonLongPress() { ampState->onButtonLongPress(); }
void onButtonFullHold() { ampState->onButtonFullHold(); }
void onKnobTurned(int8_t change) { ampState->onKnobTurned(change); }
void onRemoteVolPlus() { ampState->onRemoteVolPlus(); }
void onRemoteVolMinus(){ ampState->onRemoteVolMinus(); }
void onRemoteMute() { ampState->onRemoteMute(); }
void onRemoteSource() { ampState->onRemoteSource(); }
void onRemotePower() { ampState->onRemotePower(); }
void onTriggerRise(trigger_t source) { ampState->onTriggerRise(source); }
void onTriggerFall(trigger_t source) { ampState->onTriggerFall(source); }
void onSilence() { ampState->onSilence(); }
void onMenuExit() { ampState->onMenuExit(); }

void transitionTo(AmpState * newState) {
  ampState = newState;
  ampState->onEntry();
}

void setup() {
  Serial.begin(115200);
  //while(!Serial) delay(10);

  powerControl.begin();
  optionsSetup();
  DisplaySetup();
  BLESetup();
  knob.begin();
  ourRemote.listen();
  triggerMonitor.begin();

  //animateLogo();
  showLogo();
  delay(2000);

  if(thisUSB.Init() == -1) {
    ampDisp.displayMessage("USB didn't start.");
    ampDisp.refresh();
    while(1); // Halt
  }

  // Register callbacks.
  ourMiniDSP.attachOnInit(&onDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&onDSPVolume);
  ourMiniDSP.attachOnMutedChange(&onDSPMute);
  ourMiniDSP.attachOnSourceChange(&onDSPSource);
  //ourMiniDSP.attachOnParse(&OnParse);
  ourMiniDSP.attachOnNewInputLevels(&onDSPInputLevels);
  ourMiniDSP.attachOnNewInputGains(&onDSPInputGains);
  // Remote and knob callbacks have fixed names so they don't need to be registered.
  ourMiniDSP.callbackOnResponse();

  transitionTo(&ampOffState);
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
