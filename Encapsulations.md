# Encapsulations

## Hardware

### MiniDSP (UHS lib, with mods)
* Read state from MiniDSP
* Set DSP input and volume

### PowerControl
* Control of power to dsp and amps via relays and (if used) EN lines
* Read back status of each

# Input and power rules

## Input detection
### Direct measures
These are read directly (no persistent state unless there's filtering on the analog inputs)
* Trigger input corresponding to a signal input
* For analog, detection of signal, which means
    * Signal present
    * In absence of signal, optional detection of low impedance

### Input sense states: InputSense
The triggers are just copies of the direct measures.
AnalogSignal has a persistent state to allow a timeout.

* DTrigger (direct read)
    * ABSENT
    * PRESENT 
* ATrigger (direct read)
    * ABSENT
    * PRESENT
* AnalogSignal (state machine)
    * ABSENT
        * Signal --> PRESENT
        * LowImpedance --> PRESENT
    * PRESENT
        * !Signal --> SILENT
    * SILENT
        * HoldupTime expired --> ABSENT
        * !LowImpedance --> ABSENT
* AnalogActive = ATrigger == PRESENT || AnalogSignal != ABSENT
* DigitalActive = DTrigger

### Input state - ANALOG, DIGITAL, NA
* CurrentInput: current from MiniDSP - ANALOG, DIGITAL, NA
* PriorInput: last from MiniDSP
* ChosenInput: from inputs/state machine(s) - ANALOG, DIGITAL, NA

### Input/Power state machine: PowerState
Determination of whether the amp should be on or off based on InputSense, the input selected by the DSP (may be changed by the remote), and the panel buttons
* OFF // Respond to any source or panel activity
    * if DigitalButton || DigitalActive
        * ChosenInput = DIGITAL // Input activity determines the input
        * --> ON
    * elseif AnalogButton || AnalogActive
        * ChosenInput = ANALOG
        * --> ON
    * elseif PowerButton
        * ChosenInput = NA
        * --> ON
* ON  // Accept any input change from the remote, and monitor the chosen input
    * if ChosenInput == CurrentInput
        * ChosenInput = NA
    * if CurrentInput DIGITAL
        * if !DigitalActive --> WAIT
    * elseif CurrentInput ANALOG
        * if !AnalogActive --> WAIT
* WAIT // Shutdown in the extended absence of signal or buttons
    * if DigitalButton || DigitalActive
        * ChosenInput = DIGITAL
        * --> ON
    * elseif AnalogButton || AnalogActive
        * ChosenInput = ANALOG
        * --> ON
    * elseif PowerButton || WaitTime expired --> OFF


## Top-level state machine

### On Sequence
1. DSP power
1. Suppress output meter
1. Wait for DSP connected to USB stack
1. (optional) volume limit
1. Set input if scheduled
1. Amp power
1. Enable output meter

### State machine
This is the main state machine that takes the dsp through its startup and controls the amp power. 
* OFF
    * if PowerState != OFF --> DSP_START
* DSP_START
    * DSP relay on
    * --> DSP_WAIT
* DSP_WAIT
    * if DSP connected() --> DSP_INITIAL_STATE
* DSP_INITIAL_STATE
    * be sure we have input and volume
    * --> DSP_INIT_INPUT
* DSP_INIT_INPUT
    * if ChosenInput != NA, Set input to ChosenInput
    * --> DSP_INIT_VOLUME
* DSP_INIT_VOLUME
    * if CurrentVolume > MaxInitialVolume, set volume
    * --> AMP_ON
* AMP_ON
    * Amp power on
    * --> ON
* ON
    * if PowerState == OFF --> TURN_OFF
    * if ChosenInput != NA && ChosenInput != CurrentInput set input
* TURN_OFF
    * Amp power off
    * DSP relay off
    * --> OFF


## Summary - PowerInputControl Task()
1. Direct read inputs
1. Input sense state machines
    * Determines the input state, including persistence in case of silence on the analog input
1. Power/input select state machine
    * Based on the input states, determines whether power should be on/off and whether the dsp should be told to use a particular input
    * Provides a period during which the unit will stay on even with inputs inactive. Useful, e.g., when turning off the TV with intent to turn on the turntable.
1. Power/input control state machine
    * Based on the Power/input select, provides
        * A proper startup sequence for the dsp and amps
        * Input selection


NOTE: The output meter shouldn't operate until the amps are on, so wherever the meter update is called should check the power state.


