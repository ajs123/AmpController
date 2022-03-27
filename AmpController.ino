#include <Arduino.h>
//#include <Usb.h>
#include <MiniDSP.h>
#include <U8g2lib.h>
#include "AmpDisplay.h"

#ifdef U8X8_HAVE_HW_I2C // For this hardware (nrf52840) should just be able to include Wire.h
#include <Wire.h>
#endif

//#include <SPI.h> // This doesn't seem to matter, at least not for the Adafruit nrf52840

//ORIG USB Usb;
USB thisUSB;
MiniDSP ourMiniDSP(&thisUSB);

// Display stuff
#define LOG_FONT u8g2_font_5x7_tr //u8g2_font_7x14_tf
#define LOG_WIDTH 25 
#define LOG_HEIGHT 9

#define DIM_TIME 5000   // ms to dim the display
uint32_t brightTime;

// Analog input detect
// Any signal on the analog input should turn the unit on and keep it on
// If the connected device is super-quiet when off and provides just a teeny bit of noise when on, then this
// could be set very low so that even turning the device (such as a turntable) on will wake up the amp.
#define ANALOG_DETECT_L A1                  // Input pin
#define ANALOG_THRESH ((50 * 2^12) / 3600)  // 50 mV with 12-bit resolution and 3.6V full scale
#define ANALOG_HOLDUP 10 * 60000            // Remain on for 10 minutes after the analog input goes quiet

// Trigger input
// The external trigger should work with a nominal 5V input (e.g., from a USB connector) and also accept 12V from
// a standard AV component trigger output. It's passed through a 22K - 5.6K divider (0.20 ratio) to ensure that
// a 12V input doesn't exceed Vdd. 
#define TRIGGER_INPUT A3                                            // Input pin
#define TRIGGER_THRESH ((4000 * 2^12) * 56 / (56 + 220) / 3600 )    // Triggers at 4V

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

void analogSetup() {
  analogReadResolution(12);
  analogReference(AR_DEFAULT);  // 3.6 V
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

// For debugging: Puts the first 8 bytes of received messages on the display.
void OnParse(uint8_t * buf) {
  char bufStr[25];
  snprintf(bufStr, 25, "%02X %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
  AmpDisp.displayMessage(bufStr);
}

#define MINBARLEVEL -90
void OnNewLevels(float * levels ) {
  
  // For testing only - just display the two values
  /*
  char bufStr[25];
  snprintf(bufStr, 25, "L %6.1f,  R %6.1f", levels[0], levels[1]);
  AmpDisp.displayMessage(bufStr);
  //scheduleDim();
  */
 
  uint8_t left = max( (int) levels[0] - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
  uint8_t right = max( (int) levels[1] - MINBARLEVEL, 0) * 100 / -(MINBARLEVEL);
  AmpDisp.displayLRBarGraph(left, right, messageArea);
  //char bufStr[25];
  //snprintf(bufStr, 25, "L %6d, R%6d", left, right);
  //AmpDisp.displayMessage(bufStr);

}

// Also for debugging - Turning an LED on while in MiniDSP.task() provides a nice indicator of activity
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

  // Register callbacks.
  ourMiniDSP.attachOnInit(&OnMiniDSPConnected);
  ourMiniDSP.attachOnVolumeChange(&OnVolumeChange);
  ourMiniDSP.attachOnMutedChange(&OnMutedChange);
  ourMiniDSP.attachOnSourceChange(&OnSourceChange);
  //ourMiniDSP.attachOnParse(&OnParse);
  ourMiniDSP.attachOnNewLevels(&OnNewLevels);

  pinMode(5, INPUT_PULLUP); // Button C
  pinMode(6, INPUT_PULLUP); // Button B
  //Button A is pin 9 - used for UHS interrupt at the moment

}

bool buttonCPrev = false;     // For button debounce

// Support periodic requests in the main loop
uint32_t lastTime;
#define INTERVAL 100          // Interval between signal level requests. If shorter, automatic status updates may be missed.

// Enable a guard against starting requests too soon after connecting
bool MiniDSPConnected = false;

// DEBUG: for USB state reporting
uint16_t lastUSBState = 0xFFFF;
char strBuf[20];

bool levelsLast;

void loop() {

  //thisUSB.busprobe();  // Now called by Task() , via MAX3421E::Task()

  ledOn(LED_BLUE);
  thisUSB.Task();
  ledOff(LED_BLUE);

  // DEBUG: Report the USB state

  uint8_t taskState = thisUSB.getUsbTaskState();
  uint8_t vbusState = thisUSB.getVbusState();
  uint16_t USBState = taskState | (vbusState << 8);
  if (USBState != lastUSBState) {
    snprintf(strBuf, sizeof(strBuf), "TASK %02x   VBUS %02x", taskState, vbusState);
    AmpDisp.displayMessage(strBuf);
    lastUSBState = USBState;
  }

  //if (taskState == 0xa0) thisUSB.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE); // If error, try reset
  //if (vbusState == 0x00) thisUSB.Init();

  if (!ourMiniDSP.connected()) {
    MiniDSPConnected = false;
    return;
  }

  // Periodic requests, such as input signal levels
  #ifdef INTERVAL

  // Upon initial connection, the MiniDSP class issues a status request. When first connecting, allow time for response before issuing further requests.
  if (!MiniDSPConnected) {
    MiniDSPConnected = true;
    lastTime = millis();    // Schedule the periodic request for later
    return;
  } 

  uint32_t currentTime = millis();
  if ((currentTime - lastTime) > INTERVAL)
  {
    if (levelsLast)
    {
      ourMiniDSP.RequestStatus();
    }
    else
    {
      ourMiniDSP.RequestLevels();
    }
    levelsLast = !levelsLast;
    //ourMiniDSP.RequestLevels();
    lastTime = currentTime;
  }

#endif

  // Button C - manual status request, for testing only
  bool buttonC = !digitalRead(5);
  if (buttonC && !buttonCPrev) {
    delay(100); // Simple debounce - needs to stay down for 100 ms
    if (!digitalRead(5)) ourMiniDSP.RequestStatus();
  }
  buttonCPrev = buttonC;

  checkDim();  // Dimming timer

  //delay(50);
}
