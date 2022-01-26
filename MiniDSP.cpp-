/* Copyright (C) 2021 Kristian Sloth Lauszus and Dennis Frett. All rights reserved.

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
 */

#include "MiniDSP.h"

void MiniDSP::ParseHIDData(HID *hid __attribute__ ((unused)), bool is_rpt_id __attribute__ ((unused)), uint8_t len, uint8_t *buf) {

        constexpr uint8_t StatusInputCommand[] = {0x05, 0xFF, 0xDA};

        // Only care about valid data for the MiniDSP 2x4HD.
        Serial1.println("ParseHIDData...");
        if(HIDUniversal::VID != MINIDSP_VID || HIDUniversal::PID != MINIDSP_PID || len <= 4 || buf == nullptr)
                return;

        // Check if this is a status update.
        //
        // This presently looks for {[length], 0x05, 0xFF, 0xDA} which is the start of the response to a request to read
        // bytes from memory starting at 0xFFDA. It expects two bytes, which would correspnd to volume (in negative half dB's)
        // and mute status.
        // The prior two bytes, 0xFFD8 and 0xFFD9 contain the current filter preset, and current source, respectively.
        // We have two choices:
        //      1. Simple: Change the request to read 4 bytes starting at 0xFFD8 and extend code to retrieve preset and source
        //      2. Generalizable: Provide a memory map and use that to update whatever locations were requested
        //
        // To support reading of signal levels (uses command 0x14 - read float - at address 0x0044), we'll need to add 
        // a section to handle that response. One approach is to use a 32-bit signature for the expected response to each
        // command, i.e., {[length], [command], [addr h], [addr l]}, which can be used in a case statement or dispatch table.
        // This would be supported by a union such as
        /* 
        union response_t {
                struct {
                        uint8_t length;
                        uint8_t command;
                        uint8_t addr_h;
                        uint8_t addr_l;
                        uint8_t data[63-4];
                } bytes;
                struct {
                        uint32_t signature;
                        uint8_t data[63-4];
                } sig_data;
                uint8_t raw[64];
                }
        } response;
        */
       // Then memcpy(&data, &response, data[0] + 1) // length in response doesn't include the length byte
       // 
        // ** I think we'd need here to attend to endianness. It may help to define a command_t... **
        /*
        union command_u {
                uint8_t address_b[2];
                uint16_t address_w;
        } command_t;
        */
       // ... and use this in defining the templates.
        //
        // First byte is the length, we ignore that for now.
        if(memcmp(buf + 1, StatusInputCommand, sizeof (StatusInputCommand)) == 0) {

                // Parse data.
                // Response is of format [ length ] [ 0x05 0xFF 0xDA ] [ volume ] [ muted ].
                const auto newVolume = buf[sizeof (StatusInputCommand) + 1];
                const auto newIsMuted = (bool)buf[sizeof (StatusInputCommand) + 2];

                const auto volumeChanged = newVolume != volume;
                const auto mutedChanged = newIsMuted != muted;

                // Update status.
                volume = newVolume;
                muted = newIsMuted;

                // Call callbacks.
                if(pFuncOnVolumeChange != nullptr && volumeChanged)
                        pFuncOnVolumeChange(volume);

                if(pFuncOnMutedChange != nullptr && mutedChanged)
                        pFuncOnMutedChange(muted);
        }
};

uint32_t MiniDSP::OnInitSuccessful() {
        // Verify we're actually connected to the MiniDSP 2x4HD.
        if(HIDUniversal::VID != MINIDSP_VID || HIDUniversal::PID != MINIDSP_PID)
                return 0;

        // Request current status so we can initialize the values.
        RequestStatus();

        if(pFuncOnInit != nullptr)
                pFuncOnInit();

        return 0;
};

uint8_t MiniDSP::Checksum(const uint8_t *data, uint8_t data_length) const {
        uint16_t sum = 0;
        for(uint8_t i = 0; i < data_length; i++)
                sum += data[i];

        return sum & 0xFF;
}

void MiniDSP::SendCommand(uint8_t *command, uint8_t command_length) const {
        // Sanity check on command length.
        if(command_length > 63)
                return;

        // Message is padded to 64 bytes with 0xFF and is of format:
        // [ length (command + checksum byte) ] [ command ] [ checksum ] [ OxFF... ]

        // MiniDSP expects 64 byte messages.
        uint8_t buf[64];

        // Set length, including checksum byte.
        buf[0] = command_length + 1;

        // Copy actual command.
        memcpy(&buf[1], command, command_length);

        const auto checksumOffset = command_length + 1;

        // Set checksum byte.
        buf[checksumOffset] = Checksum(buf, command_length + 1);

        // Pad the rest.
        memset(&buf[checksumOffset + 1], 0xFF, sizeof (buf) - checksumOffset - 1);

        pUsb->outTransfer(bAddress, epInfo[epInterruptOutIndex].epAddr, sizeof (buf), buf);
}

void MiniDSP::RequestStatus() const {
        uint8_t RequestStatusOutputCommand[] = {0x05, 0xFF, 0xDA, 0x02}; // Read 2 bytes at address 0xFFDA, which are volume and mute
                        // {0x05, 0xFF, 0xD9, 0x03} would give source, volume, and mute
                        // {0x05, 0xFF, 0XD8, 0x04} would give preset, source, volume, and mute (everything on the remote)

        SendCommand(RequestStatusOutputCommand, sizeof (RequestStatusOutputCommand));
}