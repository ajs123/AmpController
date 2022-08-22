## Technical details

### Hardware
See Configuration.h for IO connections
- CPU: Adafruit Feather nRF52840 Express
- Display: Adafruit Featherwing 64x128 monochrome OLED
- USB: Universal Host Shield mini 2.0
- Remote receiver: Vishay TSOP34838
- Rotary encoder/button: Bourns PEC11-4215F-S24
- Power control relay: Generic opto-isolated relay board 
- Amp control: Open collector pull-down on the ICEPower amp /EN pins
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
The main loop() implements a state pattern, with each state defined by
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
  - Triggers (analog inputs)
    - Trigger rise
    - Trigger fall

- a requests() function that's called at 50 ms intervals. This is intended for issuing requests to the MiniDSP.

From an Off state, the basic sequence for turning on is 
1. Init the USB interface
1. Amp disable and power relay on, to power up the MiniDSP and amps
1. Await MiniDSP USB connection
1. Set source selection according to any trigger inputs
1. Set input gain according to the source (per settable option)
1. Check that volume is within limits (per settable options)
1. Unmute and enable amps

    Additional states handle timeout of the initial USB connection. Timeout of the initial USB connection causes transition to a power cycle (retry) state. A menu state is accessible from Off via a button long hold, and returns to Off.

Interaction cycle with the MiniDSP:
- Request issued at the 50 ms tick, according to the state. In the On state, the request is for input levels to drive the VU meter.
- Polls include the USB, so any response to the last request comes at an ensuing poll. 

  Callbacks from the polls can include new requests to the MiniDSP (e.g., in the On state, turning the knob triggers a request to change the volume.).

  It's not clear how the MiniDSP handles new requests that are sent prior to its response to a prior request. The MiniDSP *does* appear to act upon commands sent without waiting for a response, but our practice here is to wait for a response. Accordingly, when a callback issues a request to the MiniDSP, it sets lastTime = millis() to delay the next regular 50 ms tick.  

### Notes on the USB Host Shield library and the Maxim 3421
The Host Shield (UHS) library is pretty tangled and hard to follow. We may be departing from typical use by powering down the MiniDSP, though in initial development worked reliably while unplugging and re-plugging the MiniDSP did not. In early tests, reliabile detection/enumeration of the MiniDSP required the MiniDSP to be plugged in and powered down, and reset of the controller to precede power-up of the MiniDSP. MiniDSP connection is detected when the blue LED lights on the MiniDSP board, about 6 seconds after power is applied to the MiniDSP.

Exit from the AmpWaitDSP state is through the onDSPConnected() callback, so it requires a connection event which occurs at the end of enumeration. The UHS code used here includes a patch from tmk (see comments in usbhost.h) to accomodate occasional failure of a connection event to set the corresponding interrupt bit. In describing the patch, tmk notes that they'd never seen the chip fail to assert the bit on a disconnect. FWIW, tmk's statement is supported by the sample code provided by Maxim in its USB Laboratory:

```
void wait_for_disconnect(void)
{
printf("\nWaiting for device disconnect\n");
Hwreg(rHIRQ,bmCONDETIRQ);    				// clear the disconect IRQ
while(!(Hrreg(rHIRQ) & bmCONDETIRQ)) ; 		// hang until this changes
Hwreg(rMODE,bmDPPULLDN|bmDMPULLDN|bmHOST);	// turn off frame markers
printf("\nDevice disconnected\n\n");
HL1_OFF
HL4_OFF
}

void detect_device(void)
{
int busstate;
// Activate HOST mode & turn on the 15K pulldown resistors on D+ and D-
Hwreg(rMODE,(bmDPPULLDN|bmDMPULLDN|bmHOST)); // Note--initially set up as a FS host (LOWSPEED=0)
Hwreg(rHIRQ,bmCONDETIRQ);  // clear the connection detect IRQ
printf("Waiting for device connect\n\n");

	do 		// See if anything is plugged in. If not, hang until something plugs in
	{
	Hwreg(rHCTL,bmSAMPLEBUS);			// update the JSTATUS and KSTATUS bits
	busstate = Hrreg(rHRSL);			// read them
	busstate &= (bmJSTATUS|bmKSTATUS);	// check for either of them high	
	} 
	while (busstate==0);
    if (busstate==bmJSTATUS)    // since we're set to FS, J-state means D+ high
        {
        Hwreg(rMODE,(bmDPPULLDN|bmDMPULLDN|bmHOST|bmSOFKAENAB));  // make the MAX3421E a full speed host
		printf("Full-Speed Device Detected\n");
		printf("**************************\n");
        }
    if (busstate==bmKSTATUS)  // K-state means D- high
        {
        Hwreg(rMODE,(bmDPPULLDN|bmDMPULLDN|bmHOST|bmLOWSPEED|bmSOFKAENAB));  // make the MAX3421E a low speed host        
		printf("Low-Speed Device Detected\n");
		printf("*************************\n");
        }
}
```

The disconnect code simply waits for the connnection detect IRQ bit to be set, while the detect code requests a bus sampling (by setting SAMPLEBUS in HCTL). This is consistent with the statement in Maxim's app note on programming with the chip, which states that bus sampling is "normally" done by having the CPU set SAMPLEBUS.

There is also a patch to busprobe(), adding an optional argument to force a sampling of the bus regardless of Vbus. This was done amidst concern that *disconnect* events were occasionally being missed. It is almost certainly unnecessary and isn't used.

### Important classes
- AmpDisplay - Handles the normal display, via U8G2
- Knob and Button - Handle event detection for the knob and its pushbutton. The Knob class provides a single callback, for rotation of the knob. It uses the nRF52840 hardware quadrature decoder. The Button class takes care of debouncing and provides callbacks as listed above.
- RemoteHandler - Handles receipt of remote control codes, using the IRLib2 library's interrupt-driven detection. Any remote coding schemes that might be encountered in use can be un-commented in RemoteHandler.h. The class provides callbacks for remote buttons as listed above. The dispatch table in RemoteHandler.h specifies the callbacks and which keys can repeat (e.g., Vol +/- but not Mute or Power). Constants in RemoteHandler.h specify timing for early repeat rejection and minimum time between keys. The class also provides raw reads for use in remote learning. 
- PowerControl - Simple interface with the power relay and amp /EN signal.
- InputSensing - Provides a collection of classes for filtering of input level values received from the MiniDSP (for the VU meter and filtering of the external trigger inputs), for threshold detection (for the external trigger inputs), and for driving the clipping indicator.
- Options - Handles reading from and writing to the flash memory options store and provides access to current values from RAM. Options shouldn't really be public and non-const, but they are :-).
- OptionsMenu - Provides the menu, accessible from the Off state. Relies upon the ArduinoMenu library and its U8G2 display class. OptoinsMenu includes some alternate display classes that write directly to the display, providing a different font for the menu title and drawing a line beneath it.

### Other files
- Configuration.h - Hardware configuration (pin assignments)
- logo.h - The logo
- util.h - A few utility functions

### Helpful resources
- The MiniDSP usb protocol is documented only through reverse engineering. The best documentation is provided by [M. Rene's console app](https://github.com/mrene/minidsp-rs) in verbose mode and [documentation of the Rust crate](https://docs.rs/minidsp-protocol/0.1.4/src/minidsp_protocol/commands.rs.html) used by the app.
- The IRLib2 library has a comprehensive manual.
- The Arduino Menu library has helpful examples. 

    NOTE: The Arduino Menu U8G2 driver sets the U8G2 vertical text alignment in the U8G2 output class constructor and depends upon it remaining unchanged by others that write to the display. Rather than fix the driver (which should set the alignment prior to each print()), we set the alignment prior to entereing the menu.
- U8G2 is fully documented.