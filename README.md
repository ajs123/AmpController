## Technical details

### Hardware
See Configuration.h for IO connections
- CPU: Adafruit Feather nRF52840 Express
- Display: Adafruit Featherwing 64x128 monochrome OLED
- USB: Universal Host Shield mini 2.0
- Remote receiver: Vishay TSOP34838
- Rotary encoder/button: Bourns PEC11-4215F-S24
- Power control relay: Generic opto-isolated relay board 
### Required libraries
- Adafruit nRF52 core
  - Core libraries used are
    - Bluefruit - for BLE DFU
    - Rotary encoder
    - LittleFS, LittleFS_File, InternalFileSystem - for nonvolatile options
- U8G2 - for the Adafruit display
- [Universal Host Shield 2.0 Library](https://github.com/felis/USB_Host_Shield_2.0) - Local copy with extensive modifications to the MiniDSP driver
- [IRLib2](https://github.com/cyborg5/IRLib2) - Local copy of necessary files, with no modifications
- [Arduino Menu](https://github.com/neu-rah/ArduinoMenu) - Local copy of necessary files
### Software architecture
- The main loop() implements a state pattern, with each state defined by
  - an entry function
  - a polls function that's called every time through the loop. This is intended for polling of the hardware. Callbacks issue commands to the display (e.g., update the VU meter, volume indicator, etc.) or to the DSP (e.g., change the volume). 
  
    Callbacks include
    - MiniDSP
        - Received input levels
        - Received volume, mute, source (response to a status request or to a change command)
        - Received input channel gain (response to a change command only)
    - Rotary encoder
        - Rotation
        - Button
        - Press - a "click" - occurs when the button is released
        - Long press pending - occurs when the button has be held for ~2 seconds
        - Long press - occurs after a long press pending, when the button is released
        - Full hold - occurs when the button is held for ~2 seconds
    - Remote
        - Vol+
        - Vol-
        - Mute
        - Input
        - Power

  - a requests() function that's called at 50 ms intervals. This is intended for issuing requests to the MiniDSP.

- Interaction cycle with the MiniDSP:
  - Request issued at the 50 ms tick, according to the state. In the On state, the request is for input levels to drive the VU meter.
  - Polls include the USB, so any response to the last request comes at an ensuing poll. 

    Callbacks from the polls can include new requests to the MiniDSP (e.g., in the On state, turning the knob triggers a request to change the volume.).
  
    It's not clear how the MiniDSP handles new requests that are sent prior to its response to a prior request. The MiniDSP *does* appear to act upon commands sent without waiting for a response, but our practice here is to wait for a response. Accordingly, when a callback issues a request to the MiniDSP, it sets lastTime = millis() to delay the next regular 50 ms tick.  


### Helpful resources
- The MiniDSP usb protocol is documented only through reverse engineering. The best documentation is provided by [M. Rene's console app](https://github.com/mrene/minidsp-rs) in verbose mode and [documentation of the Rust crate](https://docs.rs/minidsp-protocol/0.1.4/src/minidsp_protocol/commands.rs.html) used by the app.
- The IRLib2 library has a comprehensive manual.
- The Arduino Menu library has helpful examples. 

    NOTE: The Arduino Menu U8G2 driver sets the U8G2 vertical text alignment in the U8G2 output class constructor and depends upon it remaining unchanged by others that write to the display. Rather than fix the driver (which should set the alignment prior to each print()), we set the alignment prior to entereing the menu.
- U8G2 is fully documented.