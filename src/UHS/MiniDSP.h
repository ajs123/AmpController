/* Original Copyright (C) 2021 Kristian Sloth Lauszus, Dennis Frett. All rights reserved.

 This software may be distributed and modified under the terms of the GNU
 General Public License version 2 (GPL2) as published by the Free Software
 Foundation and appearing in the file GPL2.TXT included in the packaging of
 this file. Please note that GPL2 Section 2[b] requires that all works based
 on this software must also be made publicly available under the terms of
 the GPL2 ("Copyleft").

 Contact information
 -------------------

 Kristian Sloth Lauszus
 Web      :  https://lauszus.com
 e-mail   :  lauszus@gmail.com

 Dennis Frett
 GitHub   :  https://github.com/dennisfrett
 e-mail   :  dennis.frett@live.com

 Extensively reworked by 
 Alan Snyder
 GitHub : https://github.com/ajs123

 */

#pragma once

#include "hiduniversal.h"

#define MINIDSP_VID 0x2752 // MiniDSP
#define MINIDSP_PID 0x0011 // MiniDSP 2x4HD

/**
 * This class implements support for the MiniDSP 2x4HD via USB.
 * Based on NodeJS implementation by Mathieu Rene:
 * https://github.com/mrene/node-minidsp and the Python implementation by Mark
 * Kubiak: https://github.com/markubiak/python3-minidsp.
 *
 * It uses the HIDUniversal class for all the USB communication.
 */

enum class source_t {
        Analog = 0,
        Toslink = 1,
        Usb = 2,
        Unset = 3
};

class MiniDSP : public HIDUniversal {
public:

        /**
         * Constructor for the MiniDSP class.
         * @param  p   Pointer to the USB class instance.
         */
        MiniDSP(USB *p) : HIDUniversal(p) {
        };

        /**
         * Used to check if a MiniDSP 2x4HD is connected.
         * @return Returns true if it is connected.
         */
        bool connected() {
                return HIDUniversal::isReady() && HIDUniversal::VID == MINIDSP_VID && HIDUniversal::PID == MINIDSP_PID;
        };

        /**
         * Used to call your own function when the device is successfully
         * initialized.
         * @param funcOnInit Function to call.
         */
        void attachOnInit(void (*funcOnInit)(void)) {
                pFuncOnInit = funcOnInit;
        };

        /**
         * Used to call your own function when receiving source data
         * The source is passed as an unsigned 8-bit integer with 0 = Analog, 1 = Toslink, 2 = USB (shouldn't occur).
         * @param funcOnSourceChange Function to call.
         */
        void attachOnSourceChange(void (*funcOnSourceChange)(source_t)) {
                pFuncOnSourceChange = funcOnSourceChange;
        }

        /**
         * Used to call your own function when receiving volume data
         * The volume is passed as an unsigned 8-bit integer that represents twice the
         * -dB value. Example: 19 represents -9.5dB.
         * @param funcOnVolumeChange Function to call.
         */
        void attachOnVolumeChange(void (*funcOnVolumeChange)(uint8_t)) {
                pFuncOnVolumeChange = funcOnVolumeChange;
        }

        /**
         * Used to call your own function when receiving the muted status
         * The muted status is passed as a boolean. True means muted, false
         * means unmuted.
         * @param funcOnMutedChange Function to call.
         */
        void attachOnMutedChange(void (*funcOnMutedChange)(bool)) {
                pFuncOnMutedChange = funcOnMutedChange;
        }

        /**
         * Used to call your own function when receiving the preset.
         * The preset will be passed as an unsinged 8-bit integer 0..3
         * @param funcOnPresetChange Function to call
         */
        void attachOnPresetChange(void (*funcOnPresetChange)(uint8_t)) {
                pFuncOnPresetChange = funcOnPresetChange;
        }

        /**
         * @brief For debug - used to call your own function when parsing a new message.
         * 
         */
        void attachOnParse(void (*funcOnParse)(uint8_t *)) {
                pFuncOnParse = funcOnParse;
        }

        /**
         * @brief Used to call your own function when new level data are available
         * 
         */
        void attachOnNewOutputLevels(void (*funcOnNewOutputLevels)(float *)) {
                pFuncOnNewOutputLevels = funcOnNewOutputLevels;
        }

        void attachOnNewInputLevels(void (*funcOnNewInputLevels)(float *)) {
                pFuncOnNewInputLevels = funcOnNewInputLevels;
        }

        void attachOnNewInputGains(void (*funcOnNewInputGains)(float *)) {
                pFuncOnNewInputGains = funcOnNewInputGains;
        }

        /**
         * Retrieve the current volume of the MiniDSP.
         * The volume is passed as an unsigned integer that represents twice the
         * -dB value. Example: 19 represents -9.5dB.
         * @return Current volume.
         */
        int getVolume() const {
                return (volume);// - volumeOffset);
        }

        /**
         * Retrieve the current volume of the MiniDSP in dB.
         * @return Current volume.
         */
        float getVolumeDB() const {
                return volume / -2.0;
        }

        /**
         * Retrieve the current muted status of the MiniDSP
         * @return `true` if the device is muted, `false` otherwise.
         */
        bool isMuted() const {
                return muted;
        }

        /**
         * @brief Retrieve the current source
         * @return 0 for analog, 1 for digital, 3 for unset (only at startup)
         */
        source_t getSource() {
                return source;
        }

        /**
         * @brief Retrieve the current volume offset
         */
        // uint8_t getVolumeOffset() {
        //         return volumeOffset;
        // }

        /**
         * @brief Retrieve the current volume offset in dB
         */
        // float getVolumeOffsetDB() {
        //         return volumeOffset / -2.0;
        // }

        /**
         * Request output levels
         * These are reported and picked up by the parser
         */
        void RequestOutputLevels() const;

        /**
         * @brief Request input levels
         * The response is picked up by the parser
         * 
         */
        void RequestInputLevels() const;

        
        /**
         * @brief Request input and output levels
         * The response is picked up by the parser
         * 
         */
        void RequestLevels() const;

        /**
         * @brief Set master volume
         * @param volume Volume in dB
         */
        void setVolume(float volume);

        /**
         * @brief Set master volume
         * @param volume Volume in MiniDSP integer steps (= -2*dB)
         */
        void setVolume(uint8_t volume);

        /**
         * @brief Set the volume offset. The offset is applied when setting the volume in the MiniDSP.
         * This provides compensation for different inputs (analog in particular) having different
         * maximum levels. 
         * @param offset
         */
        //void setVolumeOffset(uint8_t offset);

        /**
         * @brief Set mute
         * @brief muteOn true to mute the output
         */
        void setMute(bool muteOn);

        /**
         * @brief Set the input source
         * @param source
         */
        void setSource(source_t source);

        /**
         * @brief Set the Preset 
         * @param preset Preset number 0..3
         * @param reset  Uncertain usage; defaults to true which appears to be necessary to change presets
         */
        void setPreset(uint8_t preset, bool reset = true);

        /**
         * @brief Set the input gains
         * @param gains 
         */
        void setInputGains(const float * gains);

        /**
         * @brief Set the input gain. Left and right are set to the single value
         * @param gain 
         */
        void setInputGain(const float gain);

        /**
         * @brief Invoke the received data callbacks only when the corresponding values have changed
         */
        void callbackOnChange() {callbackAlways = false;}

        /**
         * @brief Invoke the received data callbacks on every response
         */
        void callbackOnResponse() {callbackAlways = true;}

        /**
         * Send the "Request status" command to the MiniDSP. The response
         * includes the current source, volume, and the muted status.
         */
        void
        RequestStatus() const;

        /**
         * @brief Request the volume from the MiniDSP
         */
        void requestVolume() const;

        /**
         * @brief Request the mute status from the MiniDSP
         */
        void requestMute() const;

        /**
         * @brief Request the current input from the MiniDSP
         */
        void requestSource() const;

        /**
         * @brief Request the current preset from the MiniDSP
         */
        void requestPreset() const;

        /**
         * @brief Request current input gains
         */
        void requestInputGains() const;

protected:
        /** @name HIDUniversal implementation */
        /**
         * Used to parse USB HID data.
         * @param hid       Pointer to the HID class.
         * @param is_rpt_id Only used for Hubs.
         * @param len       The length of the incoming data.
         * @param buf       Pointer to the data buffer.
         */
        void ParseHIDData(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);

        /**
         * Called when a device is successfully initialized.
         * Use attachOnInit(void (*funcOnInit)(void)) to call your own function.
         * This is useful for instance if you want to set the LEDs in a specific
         * way.
         */
        uint8_t OnInitSuccessful();
        /**@}*/

        /** @name USBDeviceConfig implementation */

        /**
         * Used by the USB core to check what this driver support.
         * @param  vid The device's VID.
         * @param  pid The device's PID.
         * @return     Returns true if the device's VID and PID matches this
         * driver.
         */
        virtual bool VIDPIDOK(uint16_t vid, uint16_t pid) {
                return vid == MINIDSP_VID && pid == MINIDSP_PID;
        };
        /**@}*/

private:
        /**
         * Calculate checksum for given buffer.
         * Checksum is given by summing up all bytes in `data` and returning the first byte.
         * @param data Buffer to calculate checksum for.
         * @param data_length Length of the buffer.
         */
        uint8_t Checksum(const uint8_t *data, uint8_t data_length) const;

        /**
         * Send the given MiniDSP command. This function will create a buffer
         * with the expected header and checksum and send it to the MiniDSP.
         * Responses will come in throug `ParseHIDData`.
         * @param command Buffer of the command to send.
         * @param command_length Length of the buffer.
         */
        void SendCommand(const uint8_t *command, uint8_t command_length) const;


        /** 
         * Get floating point from four bytes
         */
        float getFloatLE(const uint8_t * bytes);

        /**
         * @brief command byte sequence from floating point
         * 
         * @param buf command buffer
         * @param floater fp value
         */
        void putFloatLE(uint8_t * buf, const float floater);

        /**
         * Writes byte values to the MiniDSP.
         * @param addr Location
         * @param values Pointer to values
         * @param length Number of values
         */
        void writeBytes(int addr, uint8_t * values, uint8_t length);

        /**
         * @brief Parse the response to a direct set command
         * 
         * @param buf the response packet from the dsp
         */
        void parseDirectSetResponse(const uint8_t * buf);

        /**
         * @brief Parse the response to a byte read request
         * 
         * @param buf the response packet from the dsp
         */
        void parseByteReadResponse(const uint8_t * buf);

        /**
         * @brief Parse the response to a fload read request
         * 
         * @param buf the response packet from the dsp
         */
        void parseFloatReadResponse(const uint8_t * buf);

        /**
         * @brief Parse the response to a write to DSP values
         * 
         * @param buf the response packet from the dsp
         */
        void parseDSPWriteResponse(const uint8_t * buf);

        // Callbacks

        // Pointer to function called in onInit().
        void (*pFuncOnInit)(void) = nullptr;

        // Pointer to function called on change in the source
        void (*pFuncOnSourceChange)(source_t) = nullptr;

        // Pointer to function called when volume changes.
        void (*pFuncOnVolumeChange)(uint8_t) = nullptr;

        // Pointer to function called when muted status changes.
        void (*pFuncOnMutedChange)(bool) = nullptr;

        // Pointer to function called when the preset changes.
        void (*pFuncOnPresetChange)(uint8_t) = nullptr;

        // Pointer to function called when a new message is parsed.
        void (*pFuncOnParse)(uint8_t *) = nullptr;

        // Pointer to function called when new output level data are available.
        void (*pFuncOnNewOutputLevels)(float *) = nullptr;

        // Pointer to function called when new input level data are available.
        void (*pFuncOnNewInputLevels)(float *) = nullptr;

        // Pointer to function called when new input gains are available
        void (*pFuncOnNewInputGains)(float *) = nullptr;

        // -----------------------------------------------------------------------------

        // MiniDSP state. 

        // The volume is stored in the DSP as an unsigned integer that represents twice the
        // -dB value. Example: 19 represents -9.5dB. We use 16 bits here to allow an "unknown" value.
        uint8_t preset = 4;     // Start out with "unkown" values so that callbacks will be triggered on the first update
        source_t source = source_t::Unset;
        uint16_t volume = 0x100; 
        uint8_t muted = 2;

        // Volume offset - decreases the volume on the digital input to match the analog
        //uint8_t volumeOffset = 0;

        // Whether to invoke callbacks even if a value hasn't changed
        bool callbackAlways = true;

        float outputLevels[4] = { 0.0, 0.0, 0.0, 0.0 };

        float inputLevels[2] = { 0.0, 0.0 };

        float inputGains[2] = { -128.0, -128.0 };
};
