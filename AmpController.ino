#include <Arduino.h>
#include "src/UHS/MiniDSP.h"
#include "AmpDisplay.h"
#include "PowerControl.h"
#include "RemoteHandler.h"
#include "Button.h"
#include "Knob.h"
#include <RotaryEncoder.h>
#include "OptionsMenu.h"
#include <Wire.h>           // For the I2C display. Also provides Serial.
#include <SPI.h>            // This doesn't seem to matter, at least not for the Adafruit nrf52840
#include <bluefruit.h>      // For OTA updates
#include "Options.h"
#include "Configuration.h"
#include "logo.h"
#include "InputSensing.h"

//#define VBUS_DEBUG
//#define INCLUDE_DEBUG

// Options store
Options & ampOptions = Options::instance();                   // singleton form

// Hardware and interface class instances
USB thisUSB;                                                  // USB via Host Shield
MiniDSP ourMiniDSP(&thisUSB);                                 // MiniDSP on thisUSB
U8G2_SH1107_64X128_F_HW_I2C display(U8G2_R1, U8X8_PIN_NONE);  // Adafruit OLED Featherwing display on I2C bus
AmpDisplay ampDisp(&display);                                 // Live display on the OLED

// Input devices
//Remote ourRemote(IR_PIN);                                     // IR receiver
Remote & ourRemote = Remote::instance();                      // singleton form
Button goButton(ENCODER_BUTTON);                              // Encoder action button
Knob knob(ENCODER_A, ENCODER_B);                              // Rotary encoder

// Power relay and amp enables
PowerControl powerControl;

// Input and trigger monitoring
InputMonitor inputMonitor;
TriggerSensing triggerMonitor;
TimedTrigger<float> clipSensor(-defaultClippingHeadroom, clipIndicatorTime, LED_RED);  // Headroom will get set per the stored options

// Interval (ms) between queries to the dsp (requests() in the main loop)
// Limited mainly by the ~36 ms needed for refresh of the OLED display.
constexpr uint32_t INTERVAL = 50;   

// Persistent state
uint32_t lastTime {0};    // millis() of the last timed query

// BLE services
BLEDfu bledfu;      // Device firmware update
BLEDis bledis;      // Device information service

void BLESetup() {
  Bluefruit.begin();
  Bluefruit.setName(BLE_NAME);

  bledfu.begin();

  bledis.setManufacturer(BLE_MANUF);
  bledis.setModel(BLE_MODEL);
  if (setFirmwareString) bledis.setFirmwareRev(VERSION);
  bledis.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(bledfu);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(60);
}

void DisplaySetup() {
  display.begin();
  display.setFontMode(0);
  //ampDisp.displayMessage("START");
  ampDisp.scheduleDim();
  ampDisp.refresh();
  ampDisp.setImmediateUpdate(false);
}

#ifdef INCLUDE_DEBUG
// DEBUG: Puts the first 8 bytes of received messages on the display.
void OnParse(uint8_t * buf) {
  char bufStr[28];
  snprintf(bufStr, 28, "%02X %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
  ampDisp.displayMessage(bufStr);
}

// DEBUG: for USB state reporting
void showUSBTaskState(bool regardless = false) {
  static uint16_t lastUSBState = 0xFFFF;
  char strBuf[20];
  uint8_t taskState = thisUSB.getUsbTaskState();
  uint8_t vbusState = thisUSB.getVbusState();
  uint16_t USBState = taskState | (vbusState << 8);
  if ((USBState != lastUSBState) || regardless) {
    snprintf(strBuf, sizeof(strBuf), "TASK %02x VBUS %02x", taskState, vbusState);
    ampDisp.displayMessage(strBuf);
    ampDisp.refresh();
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
#endif

constexpr float VUCoeff = 0.32 * INTERVAL / 50; // Approximates std. VU step response 90% at 300 ms
constexpr float signalFloorDB = -128.0;

// Filtered levels for the VU meter
IIR<float> leftLevel(VUCoeff, signalFloorDB);
IIR<float> rightLevel(VUCoeff, signalFloorDB);

// Provide the gain corresponding to the identified source
float sourceGain(source_t source) {
  return (source == source_t::Analog) ? (float)ampOptions.analogDigitalDifference : 0.0;
}

// On state callback for new input levels from the MiniDSP: VU meter, silence monitor, clipping sensor
void handleInputLevels(float * levels) {
  float left = leftLevel.next(levels[1]);
  float right = rightLevel.next(levels[0]);
  inputVUMeter(left, right);
  inputMonitor.task(left, right);
  clipSensor.next(max(left, right));
  //digitalWrite(LED_RED, clipSensor.next(max(left, right)) ? HIGH : LOW);
}

/**
 * @brief Provides a VU meter based on input levels and the volume setting.
 * 
 * @param levels The two inputs, in dB.
 */
void inputVUMeter(float leftLevel, float rightLevel) {
  int intLeftLevel = round(leftLevel + ourMiniDSP.getVolumeDB());
  int intRightLevel = round(rightLevel + ourMiniDSP.getVolumeDB());
  uint8_t left = !ourMiniDSP.isMuted() ? max( intLeftLevel - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL) : 0;
  uint8_t right = !ourMiniDSP.isMuted() ? max( intRightLevel - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL) : 0;
  ampDisp.displayLRBarGraph(left, right, messageArea);
}

// Set the volume in the MiniDSP, respecting limits
void setVolume(uint8_t volume) {
  ourMiniDSP.setVolume(limit(volume, ampOptions.maxVolume, uint8_t(0xFF)));   // Unsigned int representing negative dB, so min is maxVolume
  lastTime = millis();
}

// Change volume by the specified amount
void volChange(int8_t change) {
  int currentVolume = ourMiniDSP.getVolume();
  int newVolume = currentVolume - change;     // + change is - change in the MiniDSP setting
  newVolume = limit(newVolume, int(ampOptions.maxVolume), 0xFF); //min( max(newVolume, ampOptions.maxVolume), 0xFF);
  if (newVolume != currentVolume) ourMiniDSP.setVolume(static_cast<uint8_t>(newVolume));
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  ampDisp.wakeup();
  lastTime = millis();
}

// Increase the volume by one tick
void volPlus() {
  uint8_t currentVolume = static_cast<uint8_t>(ourMiniDSP.getVolume());
  if (currentVolume > ampOptions.maxVolume) ourMiniDSP.setVolume(--currentVolume);
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  ampDisp.wakeup();   // Only really needed if already at maximum
  lastTime = millis();
}

// Decrease the volume by one tick
void volMinus() {
  uint8_t currentVolume = static_cast<uint8_t>(ourMiniDSP.getVolume());
  if (currentVolume != 0xFF) ourMiniDSP.setVolume(++currentVolume);
  if (ourMiniDSP.isMuted()) ourMiniDSP.setMute(false);
  lastTime = millis();
}

// Set the mute in the MiniDSP
void setMute(bool muted) {
  ourMiniDSP.setMute(muted);
  lastTime = millis();
}

// Toggle the mute state
void toggleMute() {
  ourMiniDSP.setMute(!ourMiniDSP.isMuted());
  lastTime = millis();
  //static bool m {false};
  //m = !m;
  //if (m) powerControl.ampDisable(); else powerControl.ampEnable();
}

// Set the source in the MiniDSP
void setSource(source_t source) {
  ourMiniDSP.setSource(source);
  //ourMiniDSP.setVolumeOffset(source == source_t::Analog ? 0 : ampOptions.analogDigitalDifference);
  lastTime = millis();
}
// Identify the currently unselected
source_t flipSource() {
  source_t newSource = ourMiniDSP.getSource() == source_t::Analog ? source_t::Toslink : source_t::Analog;
  return newSource;
}

// Set the input gains in the MiniDSP (L/R to the same value)
void setInputGain(source_t source) {
  //const float aGains[] = {6.0, 6.0};
  //const float dGains[] = {-40.0, 0.0};
  //if (source == source_t::Toslink) ourMiniDSP.setInputGains(dGains);
  ourMiniDSP.setInputGain(sourceGain(source));
  lastTime = millis();
}

// Show the preset, using the volume area
void showPreset(const uint8_t preset) {
  ampDisp.preset(preset + 1);
  ampDisp.refresh();
  ampDisp.undim();
}

void optionsSetup() {
  ampOptions.begin();
  ampOptions.load();
  ourRemote.loadFromOptions();
}

void showLogo(bool version = false) {
  display.clear();
  display.drawXBMP(0, (64 - logo_height) / 2, logo_width, logo_height, logo_bits);
  if (version) ampDisp.displayMessage(VERSION);
  display.updateDisplay();
}

// The main state machine - uses a classic state pattern.
// Each state has 
//   entry(), invoked when switching to the state 
//   poll(), invoked each time around the loop,
//   request(), invoked on each 50 ms tick, and
//   a callback for each possible event. 
// Naming convention for callbacks is on<Source><Event>(...)

// Virtual class for states
class AmpState {
  public:
    virtual void onEntry(){}
    virtual void polls(){}
    virtual void requests(){}

    virtual void onDSPConnected(){}
    virtual void onDSPVolume(uint8_t volume){}
    virtual void onDSPMute(bool mute){}
    virtual void onDSPSource(source_t source){}
    virtual void onDSPPreset(uint8_t preset){}
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
    virtual void onRemotePreset(){}
    virtual void onKnobTurned(int8_t change){}
    virtual void onTriggerRise(trigger_t source){}
    virtual void onTriggerFall(trigger_t source){}
    virtual void onSilence(){}
    virtual void onMenuExit(){}
};

// Transition to the identified state
void transitionTo(AmpState * newState);

#ifdef VBUS_DEBUG
// VBUS_DEBUG code for automatically doing N power cycles
const int testCycles = 500;
int cycleCount {0};
int offStateExtras {0};
int initCount {0};
int powerCycles {0};

void showDebugData() {
  Serial.printf("N %d E %d I %d P %d\n", cycleCount, offStateExtras, initCount, powerCycles);
  // char buf[30];
  // snprintf(buf, 25, "N %d E %d I %d P %d", cycleCount, offStateExtras, initCount, powerCycles);
  // ampDisp.displayMessage(buf);
  // ampDisp.refresh();
}
// 
#endif

// The Off state - power down, then just watch the remote, button, and triggers
const uint32_t minOffTime = 1000;
class AmpOffState : public AmpState {
  uint32_t entryTime {0};
  void onEntry() override {
    entryTime = millis();
    powerControl.ampDisable();
    powerControl.powerOff();
    display.clear();
    display.updateDisplay();
    ourRemote.stopListening(); // Empty the receive buffer
    ourRemote.listen();
  }
  void polls() override {
    if (thisUSB.getVbusState() != SE0) thisUSB.Task();  // After powerOff, continue polling the USB until we see the disconnect
    //thisUSB.Task();
    uint32_t currentTime = millis();
    if ((currentTime - entryTime) > minOffTime) {       // Avoid re-applying power too soon
      // if (thisUSB.getVbusState() != SE0) {
      //   thisUSB.busprobe(true);  // Just in case the chip missed the disconnect - This appears never to be reached
      //   #ifdef VBUS_DEBUG
      //   offStateExtras++;
      //   #endif
      // }
      ourRemote.Task();
      goButton.Task();
      triggerMonitor.task();
      entryTime = currentTime + minOffTime;         // Keep working 45 days later!
      #ifdef VBUS_DEBUG
      showDebugData();
      if (cycleCount < testCycles) {
        ++cycleCount;
        onButtonShortPress();
      }
      #endif
    }
  }
  void onButtonFullHold() override;                 // --> Menu - See the transition table
  void onButtonShortPress() override;               // --> Start seq - See transition table

  void onTriggerRise(trigger_t sources) override;   // --> Start seq - See transition table

  void onRemotePower() override;                    // --> Start seq - See transition table below
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

// DSP wait state - power up and watch for the DSP to become connected.
const uint32_t maxDSPStartupTime = 10000; // ms. Normal MiniDSP startup is about 6 seconds
class AmpWaitDSPState : public AmpState {
  uint32_t entryTime {0};
  void onEntry() override {
    showLogo();
    #ifdef VBUS_DEBUG
    showDebugData();
    #endif
    entryTime = millis();
    thisUSB.Task();                       // Just in case we enter this state from other than AmpOff
    if (thisUSB.getVbusState() != SE0) {  // This should have been satisfied while in AmpOff - and it appears that it reliably is
      //showUSBTaskState(true);
      #ifdef VBUS_DEBUG
      initCount++;
      #endif
      thisUSB.Init();
      thisUSB.busprobe(true);
      ampDisp.displayMessage("0");
    } else {
      ampDisp.displayMessage(".");
    }
    powerControl.ampDisable();
    powerControl.powerOn();
    ampDisp.refresh();
    ampDisp.undim();
  }

  void onDSPTimeout();                              // --> Cycle power - See transition table
  void polls() override {
    thisUSB.Task();
    //showUSBTaskState();
    if ((millis() - entryTime) > maxDSPStartupTime) onDSPTimeout();
  }
  void onDSPConnected() override;                   // --> Source check - See transition table
} ampWaitDSPState;

// DSP timeout state - power down for 2 sec and retry
const uint32_t DSPPowerDownTime = 2000; // ms
class AmpCyclePowerState : public AmpState {
  uint32_t entryTime {0};
  void onEntry() override {
    entryTime = millis();
    powerControl.powerOff();
    #ifdef VBUS_DEBUG
    powerCycles++;
    Serial.printf("Timeout on cycle %d with Task state %02X and Vbus state %02X\n", cycleCount, thisUSB.getUsbTaskState(), thisUSB.getVbusState());
    #endif
    ampDisp.displayMessage("*");
    ampDisp.refresh();
  }
  void onTime();                                    // --> Wait for DSP - see transition table
  void polls() override {
    thisUSB.Task();
    if ((millis() - entryTime) > DSPPowerDownTime) {
      thisUSB.Init();  
      thisUSB.busprobe(true);
      onTime();
    }
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
    ourMiniDSP.requestSource(); 
  }

  void toSetGain();
  void onDSPSource(source_t source) override {
    // If a desired source is set, check input against that
    if (desiredSource != source_t::Unset) {
      if (source == desiredSource) {
        toSetGain();
        desiredSource = source_t::Unset;
      } else {
        setSource(desiredSource);
      }
      return;
    }
    // Otherwise, check input against what's called for by the triggers
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

// Set gain state - ensure that input gains match the options settings
class AmpSetGainState : public AmpState {
  void onEntry() override { 
    ampDisp.displayMessage("..."); 
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
    float reqGain = sourceGain(ourMiniDSP.getSource());
    if (fEqual(gains[1], reqGain) && fEqual(gains[1], reqGain)) toWaitVolume();   
  }
} ampSetGainState;

// Volume wait state - ensure that the volume is within range (generally, and for startup)
// Placing this in the sequence after the source change means that the startup volume limit applies
// whenever the source is changed.
class AmpWaitVolumeState : public AmpState {
  void onEntry() override { 
    ampDisp.displayMessage("...."); 
    ampDisp.refresh();
    }
  void polls() override {
    thisUSB.Task();
  }
  void requests() override {
    ourMiniDSP.requestVolume(); 
  }
  void toWaitMute();
  
  void onDSPVolume(uint8_t volume) override {
    if (volume < ampOptions.maxInitialVolume) setVolume(ampOptions.maxInitialVolume);
    else toWaitMute();                              // --> Mute check - See transition table
  }
} ampWaitVolumeState;

// Mute wait state - ensure that the DSP is unmuted
class AmpWaitMuteState : public AmpState {
  void onEntry() override { 
    ampDisp.displayMessage("....."); 
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
    clipSensor.setThreshold(-(float)ampOptions.clippingHeadroom);
    display.clear();
    ampDisp.source((source_t) ourMiniDSP.getSource());
    ampDisp.volume(-ourMiniDSP.getVolume()/2.0);
    ampDisp.mute(ourMiniDSP.isMuted());
    ourRemote.stopListening();  // Empty the receive buffer
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
    #ifdef VBUS_DEBUG
    transitionTo(&ampOffState);
    #endif
  };

  void requests() override { ourMiniDSP.RequestInputLevels(); }

  void onDSPVolume(uint8_t volume) override { ampDisp.volume(-volume/2.0); }
  void onDSPMute(bool isMuted) { ampDisp.mute(isMuted); }
  void onDSPSource(source_t source) { ampDisp.source((source_t) source); }
  void onDSPInputLevels(float * levels) { handleInputLevels(levels); }

  void toOff();
  void toSource();

  void onButtonShortPress() override { toggleMute(); }
  void onButtonLongPressPending() override { ampDisp.cueLongPress(); }
  void onButtonLongPress() override { 
    ampWaitSourceState.setDesiredSource(flipSource());
    toSource();
  }
  void onButtonFullHold() override;                         // --> Off - See transition table

  void onRemoteVolPlus() override { volPlus(); }
  void onRemoteVolMinus() override { volMinus(); }
  void onRemoteMute() override { toggleMute(); }
  void onRemoteSource() override { 
    ampWaitSourceState.setDesiredSource(flipSource());
    toSource();
  }
  void onRemotePreset() override;                           // --> Choose preset - See transition table
  void onRemotePower() override;                            // --> Off - See transition table

  void onKnobTurned(int8_t change) override { volChange(change); }

  void onTriggerFall(trigger_t triggers) override {
    // triggers indicates which has fallen
    source_t source = ourMiniDSP.getSource();
    if (triggers.analog && (source == source_t::Analog)) {
      if (!triggerMonitor.getTriggers().digital) {
        clipSensor.clearIndicator();
        toOff();                                            // --> Off - see transition table
        return;
      }
      toSource();                                           // --> Switch source - see transition table
    }
    if (triggers.digital && (source == source_t::Toslink)) {
      if (!triggerMonitor.getTriggers().analog) {
        clipSensor.clearIndicator();
        toOff();                                            // --> Off - see transition table
        return;
      }
      toSource();                                           // --> Switch source - see transition table
    }
  }

  void onSilence() override {
    if (ourMiniDSP.getSource() == source_t::Analog) {
      if (triggerMonitor.getTriggers().analog) return;  // Ignore silence when trigger present
      if (silenceSourceChange && triggerMonitor.getTriggers().digital) {
        toSource();                                     // Silence with other trigger present
        return;
      }
      clipSensor.clearIndicator();
      toOff();                                          // Silence with no triggers
      return;
    } else{
      if (triggerMonitor.getTriggers().digital) return;
      if (silenceSourceChange && triggerMonitor.getTriggers().analog) {
        toSource();
        return;
      }
      clipSensor.clearIndicator();
      toOff();
      return;
    }
  };                      

} ampOnState;

// Set preset state - set the new preset
const uint32_t SET_PRESET_TIMEOUT {4000};   // ms. Normal response is about 2 seconds
class AmpSetPreState : public AmpState {

  private:
    uint8_t newPreset {4};
    uint32_t setTime {0};

  public:
    void setDesiredPreset(uint8_t preset) {
      newPreset = preset % 4;
    }

  private:
    void toOn();
    void onEntry() override {
      setTime = millis();
      if (newPreset > 3) toOn();
      else ourMiniDSP.setPreset(newPreset, true);
    }

    void polls() override {
      thisUSB.Task();
      if ((millis() - setTime) > SET_PRESET_TIMEOUT) {
        setTime = millis();
        ourMiniDSP.setPreset(newPreset, true);
      }
    }

    void onDSPPreset(uint8_t preset) override;      // --> ampOnState - see transition table

} ampSetPreState;

// Choose preset state - display the current preset and increment with each remote press. 
class AmpChoosePreState : public AmpState {

  uint8_t currentPreset {4};  // Presets are 0..3
  uint8_t newPreset {4};
  uint32_t lastTime {0};

  void onEntry() override {
    Serial.println("Entered ChoosePreState.");
    lastTime = millis();
    currentPreset = 4;     
    ampDisp.displayMessage("", messageArea); // No VU display 
    ampDisp.refresh();
  }

  void toSetPreset();
  void toOn();
  void polls() override {
    thisUSB.Task();
    ourRemote.Task();
    if ((millis() - lastTime) > CHOOSE_PRESET_TIMEOUT) {
      if (newPreset != currentPreset) {
        ampSetPreState.setDesiredPreset(newPreset);
        toSetPreset();
      }
      else {
        toOn();
      }
    }
  }

  void requests() override {
    if (currentPreset > 3) ourMiniDSP.requestPreset();
  }

  void onDSPPreset(uint8_t preset) {
    currentPreset = preset;
    newPreset = preset;
    showPreset(preset);
  }

  void onRemotePreset() {
    if (newPreset > 3) return;      // Not until we've gotten the preset from the MiniDSP
    newPreset = (newPreset + 1) % 4;
    showPreset(newPreset);
    lastTime = millis();
  }

} ampChoosePreState;



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
void AmpOnState::         onRemotePreset()          { transitionTo(&ampChoosePreState); }
void AmpChoosePreState::  toSetPreset()             { transitionTo(&ampSetPreState); }      // timeout when a new preset has been chosen
void AmpChoosePreState::  toOn()                    { transitionTo(&ampOnState); }          // timeout if the preset hasn't been changed
void AmpSetPreState::     onDSPPreset(uint8_t)      { transitionTo(&ampOnState); }
void AmpSetPreState::     toOn()                    { transitionTo(&ampOnState); }          // on entry without setting intended preset

// The state pattern context

AmpState * ampState {&ampOffState}; 

void polls() { ampState->polls(); }
void requests() { ampState->requests(); }
void onDSPConnected() { ampState->onDSPConnected(); }
void onDSPVolume(uint8_t volume) { ampState->onDSPVolume(volume); }
void onDSPMute(bool mute) { ampState->onDSPMute(mute); }
void onDSPSource(source_t source) { ampState->onDSPSource(source); }
void onDSPPreset(uint8_t preset) { ampState->onDSPPreset(preset); }
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
void onRemotePreset() { ampState->onRemotePreset(); }
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
  if (goButton.rawClosed()) BLESetup();
  knob.begin();
  ourRemote.listen();
  triggerMonitor.begin();

  //animateLogo();
  showLogo(true);
  delay(2000);

  // Init is called with each entry to WaitDSP, so it's possibly not needed here.
  // But it's helpful to know at power-on if something's wrong with the UHS
  if(thisUSB.Init() == -1) {
    ampDisp.displayMessage("USB didn't start.");
    ampDisp.refresh();
    while(1); // Halt
  }

  // Register callbacks.
  ourMiniDSP.attachOnInit(&onDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&onDSPVolume);
  ourMiniDSP.attachOnMutedChange(&onDSPMute);
  ourMiniDSP.attachOnPresetChange(&onDSPPreset);
  ourMiniDSP.attachOnSourceChange(&onDSPSource);
  //ourMiniDSP.attachOnParse(&OnParse);         // Only for debugging
  ourMiniDSP.attachOnNewInputLevels(&onDSPInputLevels);
  ourMiniDSP.attachOnNewInputGains(&onDSPInputGains);
  // Remote and knob callbacks have fixed names so they don't need to be registered.

  ourMiniDSP.callbackOnResponse();              // We want a callback even if the value is unchanged

  lastTime = millis();

  transitionTo(&ampOffState);
}

void loop() {
  polls();

  uint32_t currentTime = millis();
  if ((currentTime - lastTime) >= INTERVAL) {
    requests();
    lastTime = currentTime;
  }
}
