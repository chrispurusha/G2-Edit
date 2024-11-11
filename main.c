/*
 * The G2 Editor application.
 *
 * Copyright (C) 2024 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * Reference credit on some of the excellent G2 comms protocol work by
 * Bruno Verhue in his Delphi editor application:
 *
 * https://www.bverhue.nl/g2dev/
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include "iokit.h"
#include "utils.h"
#include "defs.h"


typedef enum
{
    eStateNone,
    eStateFindDevice,
    eStateInit,   // Items from here increment in order for init sequence states
    eStateStop,
    eStateGetSynthSettings,
    eStateGetUnknown1,
    eStateGetUnknown2,
    eStateSelectSlot,
    eStateGetPatchVersion,
    eStateGetPatchSlot,
    eStateGetPatchNameSlot,
    eStateStart,  // Last one to increment in order
    eStatePoll,
    eStateExit
} eState;

static bool    gotBadConnectionIndication = false;
static bool    gotPatchChangeIndication   = false;
static uint8_t slotVersion[4]             = {0};


static int parse_synth_settings(uint8_t * buff, int length)
{
    int      retVal  = EXIT_FAILURE;
    int      i       = 0;
    uint32_t bitPos  = 0;
    uint8_t  ch      = 0;
    
    if (buff == NULL)
    {
        return retVal;
    }
    
    printf("Clavia string '");
    for (i=0; i<11; i++)
    {
        ch = read_bit_stream(buff, &bitPos, 8);
        if ((ch <= 0x7f) && (ch >= 0x20))
        {
            printf("%c", ch);
        }
    }
    printf("'\n");

    printf("Perf Mode 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Perf Bank 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Perf Location 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Memory Protect (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 7);
    printf("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8)); // 16 = off/disabled
    printf("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Global chan 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Sysex ID 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Local on (bit 0)0x%x\n", read_bit_stream(buff, &bitPos, 1));  //Actually top bit = 0x80
    read_bit_stream(buff, &bitPos, 7);
    read_bit_stream(buff, &bitPos, 6);
    printf("Prog Change Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    printf("Prog Change Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 6);
    printf("Controllers Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));  // send actually bit 0
    printf("Controllers Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));  // send actually bit 0
    printf("Send Clock (bit 1), ignore ext clock (bit 2) , 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Tune cent 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Global Shift Active (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Global Octave Shift 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Tune semi 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Filler 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Pedal Polarity (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8)); //0xC0 (1100) = closed, 0x40 (0100) = open therefore bit 7
    printf("Control Pedal Gain 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    //17 other unknown bytes to go
    
    retVal = EXIT_SUCCESS;
    
    return retVal;
}


static int parse_patch(uint8_t * buff, int length)
{
    int      retVal      = EXIT_FAILURE;
    int      i           = 0;
    uint8_t  type        = 0;
    int16_t  count       = 0;
    uint32_t bitPos      = 0;
    uint32_t subBitPos   = 0;
    uint32_t moduleCount = 0;
    uint32_t cableCount  = 0;
    uint32_t items       = 0;
    uint8_t  ch          = 0;
    
    if (buff == NULL)
    {
        return retVal;
    }
    
    while (BIT_TO_BYTE(bitPos) < length)
    {
        type  = read_bit_stream(buff, &bitPos, 8);
        count = read_bit_stream(buff, &bitPos, 16);
        
        subBitPos = bitPos;
        
        printf("Type = 0x%x, Count = %d\n", type, count);
        
        switch (type)
        {
            case 0x4a:
                printf("Module list\n");

                printf("Location       0x%x\n", read_bit_stream(buff, &subBitPos, 2));
                moduleCount = read_bit_stream(buff, &subBitPos, 8);
                printf("Module Count   %d\n",   moduleCount);
                for (i=0; i<moduleCount; i++)
                {
                    printf("%d\n", i);
                    printf(" Module Type   0x%x\n", read_bit_stream(buff, &subBitPos, 8));
                    printf(" Module Index  %d\n",   read_bit_stream(buff, &subBitPos, 8));
                    printf(" Column        %d\n",   read_bit_stream(buff, &subBitPos, 7));
                    printf(" Row           %d\n",   read_bit_stream(buff, &subBitPos, 7));
                    printf(" Colour        0x%x\n", read_bit_stream(buff, &subBitPos, 8));
                    printf(" Up Rate       0x%x\n", read_bit_stream(buff, &subBitPos, 1));
                    printf(" Is Led        0x%x\n", read_bit_stream(buff, &subBitPos, 1));
                    printf(" Unknown1      0x%x\n", read_bit_stream(buff, &subBitPos, 6));
                    printf(" Mode Count    %d\n",   read_bit_stream(buff, &subBitPos, 4));
                }
                printf("bitPos offset = %d, bytes = %d, count = %d\n", subBitPos-bitPos, BIT_TO_BYTE(subBitPos-bitPos), count);
                break;
            case 0x52:
                printf("Cable list\n");
                
                printf("Location       0x%x\n", read_bit_stream(buff, &subBitPos, 2));
                printf("Unknown        0x%x\n", read_bit_stream(buff, &subBitPos, 12));
                cableCount = read_bit_stream(buff, &subBitPos, 10);
                printf("Cable Count    %d\n",   cableCount);
                for (i=0; i<cableCount; i++)
                {
                    printf("%d\n", i);
                    printf(" Colour         0x%x\n", read_bit_stream(buff, &subBitPos, 3));
                    printf(" Module From    %d\n",   read_bit_stream(buff, &subBitPos, 8));
                    printf(" Connector From %d\n",   read_bit_stream(buff, &subBitPos, 6));
                    printf(" Link Type      0x%x\n", read_bit_stream(buff, &subBitPos, 1));
                    printf(" Module To      %d\n",   read_bit_stream(buff, &subBitPos, 8));
                    printf(" Connector To   %d\n",   read_bit_stream(buff, &subBitPos, 6));
                }
                break;
            case 0x4d:
                printf("Param list\n");
                break;
            case 0x5b:
                printf("Param names\n");
                for (i=0; i<count; i++)
                {
                    ch = read_bit_stream(buff, &subBitPos, 8);
                    if ((ch >= 0x20) && (ch <= 0x7f))
                    {
                        printf("%c", ch);
                    }
                }
                printf("\n");
                break;
            case 0x5a:
                printf("Module names\n");
                //Possibly id, count, string index
                printf("ID 0x%x\n", read_bit_stream(buff, &subBitPos, 8));
                items = read_bit_stream(buff, &subBitPos, 8);
                printf("Items 0x%x\n", items);
                for (i = 0; i < items; i++)
                {
                    printf("String Index 0x%x\n", read_bit_stream(buff, &subBitPos, 8));
                    printf("'%s'\n", &buff[BIT_TO_BYTE(subBitPos)]);
                    subBitPos += BYTE_TO_BIT((strlen((char *)&buff[BIT_TO_BYTE(subBitPos)]))+1);
                }
                break;
            case 0x2d:
                // This 0x2d 0x00 sequence reportedly only appears on USB comms, not in files
                // so ignore as a size by moving back a byte
                count = -1;
                break;
            default:
                printf("Unprocess type 0x%02x\n", type);
                break;
        }
        bitPos += BYTE_TO_BIT(count);
    }
    
    retVal = EXIT_SUCCESS;
    
    return retVal;
}


static int parse_patch_version(uint8_t * buff, int length)
{
    int      retVal  = EXIT_FAILURE;
    uint32_t bitPos  = 0;
    uint8_t  slot    = 0;
    uint8_t  version = 0;
    
    if (buff == NULL)
    {
        return retVal;
    }
    
    slot    = read_bit_stream(buff, &bitPos, 8);
    version = read_bit_stream(buff, &bitPos, 8);
    
    slotVersion[slot] = version;
    
    retVal = EXIT_SUCCESS;
    
    return retVal;
}


static int parse_incoming(uint8_t * buff, int length)
{
    int      retVal          = EXIT_FAILURE;
    int      i               = 0;
    uint32_t bitPos          = 0;
    uint8_t  responseType    = 0;
    uint8_t  commandResponse = 0;
    uint8_t  version         = 0;
    uint8_t  subCommand      = 0;
    uint8_t  ch              = 0;

    if (buff == NULL)
    {
        return retVal;
    }
    
    if (length > 0)
    {
#if 0
        printf("Incoming len=%d  ", length);
        for (i = 0; i < length; i++)
        {
           printf("%02x ", buff[i]);
        }
        printf("\n");
#endif
        bitPos = 0;
        responseType = read_bit_stream(buff, &bitPos, 8);
        
        switch (responseType)
        {
            case RESPONSE_TYPE_INIT:
                printf("Got Response init\n");
                retVal = EXIT_SUCCESS;
                break;
            case RESPONSE_TYPE_COMMAND:
                commandResponse = read_bit_stream(buff, &bitPos, 8);
                version         = read_bit_stream(buff, &bitPos, 8);
                subCommand      = read_bit_stream(buff, &bitPos, 8);
                
                //printf("Command response 0x%02x\n", commandResponse);
                //printf("Sub command 0x%02x\n", subCommand);
                switch (commandResponse)
                {
                    case 0x00:
                        switch (subCommand)
                        {
                            case SUB_COMMAND_VOLUME_INDICATOR:
                                printf("Got Volume ");
                                for (i = 4; i < ((length - 4)-2); i+=2)
                                {
                                    printf("%u ", read_bit_stream(buff, &bitPos, 16));
                                }
                                printf("\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            case SUB_COMMAND_LED_DATA:
                                printf("Got LED data ");
                                for (i = 4; i < ((length - 4)-2); i+=2)
                                {
                                    printf("%u ", read_bit_stream(buff, &bitPos, 16));
                                }
                                printf("\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            case 0x7e:
                                printf("Got Error!!!\n");
                                break;
                            case 0x72:
                                printf("Got resources in use\n");
                                break;
                            default:
                                printf("Got 0x00 Unknown sub command 0x%02x\n", subCommand);
                                break;
                        }
                        break;
                    case 0x0c:
                        switch (subCommand)
                        {
                            case SUB_COMMAND_OK:
                                printf("Got OK\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            case SUB_COMMAND_GET_PATCH_VERSION:
                                printf("Got get patch version\n");
                                retVal = parse_patch_version(&buff[BIT_TO_BYTE(bitPos)], length-(BIT_TO_BYTE(bitPos))-CRC_BYTES);
                                break;
                            case 0x03:
                                printf("Got synth settings\n");
                                retVal = parse_synth_settings(&buff[BIT_TO_BYTE(bitPos)], length-(BIT_TO_BYTE(bitPos))-CRC_BYTES);
                                break;
                            case 0x80:
                                printf("Got response to deliberate unknown message 1\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            case 0x1e:
                                printf("Got response to deliberate unknown message 2\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            default:
                                printf("Got 0x0c Unknown sub command 0x%02x\n", subCommand);
                                break;
                        }
                        break;
                    case 0x04:
                        switch (subCommand)
                        {
                            case 0x38:
                                printf("Got Patch load\n");
                                gotPatchChangeIndication = true;
                                retVal = EXIT_SUCCESS;
                                break;
                            case 0x05:
                                printf("Got unknown sub command 0x05 - possibly assigned voices\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            case 0x04:
                                printf("Got unknown sub command 0x04\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            case 0x29:
                                printf("Got performance name\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            default:
                                printf("Got 0x04 Unknown sub command 0x%02x\n", subCommand);
                                break;
                        }
                        break;
                    case 0x08:
                        switch (subCommand)
                        {
                            case 0x36:
                                printf("Got Patch Version\n");
                                //int pos = 0;
                                //pos = HEADER_SUB_COMMAND+1;
                                printf("Val 1 0x%02x\n", read_bit_stream(buff, &bitPos, 8));
                                printf("Val 2 0x%02x\n", read_bit_stream(buff, &bitPos, 8));
                                printf("Val 3 0x%02x\n", read_bit_stream(buff, &bitPos, 8));
                                retVal = EXIT_SUCCESS;
                                break;
                            case 0x21:
                                printf("Got Patch info\n");
                                // Note this one is a special case. Include the sub-command
                                // for parsing via -1 on pointer to first byte and +1 to
                                // length
                                retVal = parse_patch(&buff[BIT_TO_BYTE(bitPos)-1], (length-(BIT_TO_BYTE(bitPos))-CRC_BYTES)+1);
                                break;
                            case 0x27:
                                printf("Got Patch name '");
                                
                                //pos = HEADER_SUB_COMMAND+1;
                                for (i=0; i<16; i++)
                                {
                                    ch = read_bit_stream(buff, &bitPos, 8);
                                    if (ch <= 0x7f)
                                    {
                                        printf("%c", ch);
                                    }
                                }
                                printf("'\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            case 0x7f:
                                printf("Got OK\n");
                                retVal = EXIT_SUCCESS;
                                break;
                            default:
                                printf("Got 0x08 Unknown sub command 0x%02x\n", subCommand);
                                break;
                        }
                        break;
                    default:
                        printf("Got Unknown command response 0x%02x\n", commandResponse);
                        break;
                }
                break;
            default:
                printf("Got Unknown response type 0x%02x\n", responseType);
                break;
        }
    }
    
    return retVal;
}


static int rcv_extended(int dataLength)
{
    int      retVal                      = EXIT_FAILURE;
    uint32_t bitPos                      = 0;
    uint8_t  buff[EXTENDED_MESSAGE_SIZE] = {0};  // Could malloc this based on dataLength
    int      readLength                  = 0;
    int      tries                       = 0;
    uint8_t  responseType                = 0;
    
    for (tries = 0; tries < 5; tries++)
    {
        memset(buff, 0, sizeof(buff));
        readLength = read_usb_extended(buff, sizeof(buff));
        if (readLength > 0)
        {
            responseType = buff[0];
            if ((responseType == RESPONSE_TYPE_INIT) || (responseType == RESPONSE_TYPE_COMMAND))
            {
                break;
            }
        }
    }

    if (readLength == dataLength)
    {
        bitPos = BYTE_TO_BIT(dataLength-2);
        if (calc_crc16(buff, dataLength-2) == read_bit_stream(buff, &bitPos, 16))
        {
            retVal = parse_incoming(buff, dataLength);
        }
        else
        {
            printf("Bad CRC on ext message\n");
            retVal = -1;
        }
    }
    else
    {
        printf("Read ext problem! Read length = %d, data length = %d\n", readLength, dataLength);
    }
    
    return retVal;
}


static int int_rec(void)
{
    int      retVal                       = EXIT_FAILURE;
    uint32_t bitPos                       = 0;
    uint8_t  buff[INTERRUPT_MESSAGE_SIZE] = {0};
    int      readLength                   = 0;
    int      dataLength                   = 0;
    int      type                         = 0;
    int      i                            = 0;
    int      tries                        = 0;
    bool     foundNoneZero                = false;
    
    for (tries = 0; tries < 5; tries++)
    {
        memset(buff, 0, sizeof(buff));
        readLength = read_usb_interrupt(buff, sizeof(buff));
        if (readLength > 0)
        {
            break;
        }
    }
    
    if (readLength > 0)
    {
        dataLength = read_bit_stream(buff, &bitPos, 4);
        type       = read_bit_stream(buff, &bitPos, 4);
        
        if (type == RESPONSE_TYPE_EXTENDED)
        {
            // Work-around for what looks like a bug on Apple silicon only - int response looks like a
            // ext response, so check if anything other than 0s from 4th byte and don't process
            // if it's not all 0
            for (i=3; i<readLength; i++)
            {
                if (buff[i] != 0)
                {
                    foundNoneZero = true;
                    break;
                }
            }
            
            if (foundNoneZero == false)
            {
                // Extended message expected
                dataLength = read_bit_stream(buff, &bitPos, 16);
                
                retVal = rcv_extended(dataLength);
            }
        }
        else if (type == RESPONSE_TYPE_EMBEDDED)
        {
            // If Embedded, can call the process incoming, but skip the first byte
            retVal = parse_incoming(buff+1, dataLength);
        }
    }
    else if (readLength < 0)
    {
        gotBadConnectionIndication = true;
    }
    
    return retVal;
}


static int send_command(int state)
{
    int      retVal                  = EXIT_FAILURE;
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint16_t crc                     = 0;
    int      msgLength               = 0;
    int      pos                     = COMMAND_OFFSET;
    
    switch (state)
    {
        case eStateInit:
            buff[pos++] = 0x80;
            break;
        case eStateStop:
        case eStateStart:
        case eStateSelectSlot:
        case eStateGetPatchVersion:
        case eStateGetSynthSettings:
        case eStateGetUnknown1:
        case eStateGetUnknown2:
        case eStateGetPatchSlot:
        case eStateGetPatchNameSlot:
            buff[pos++] = 0x01;
            
            switch (state)
            {
                case eStateStop:
                case eStateStart:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = 0x7d;
                    switch (state)
                    {
                        case eStateStop:
                            buff[pos++] = 0x01;
                            break;
                        case eStateStart:
                            buff[pos++] = 0x00;
                            break;
                        default:
                            printf("Unknown state %d\n", state);
                            break;
                    }
                    break;
                case eStateSelectSlot:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x00;
                    buff[pos++] = 0x09; // Sub command
                    buff[pos++] = 0x00;
                    break;
                case eStateGetPatchVersion:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = 0x35;
                    buff[pos++] = 0x00; // Slot 0=A
                    break;
                case eStateGetSynthSettings:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = 0x02;
                    break;
                case eStateGetUnknown1:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = 0x81;
                    break;
                case eStateGetUnknown2:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x00;
                    buff[pos++] = 0x59;
                    break;
                case eStateGetPatchSlot:
                    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | 0;
                    buff[pos++] = slotVersion[0];
                    buff[pos++] = 0x3c;
                    break;
                case eStateGetPatchNameSlot:
                    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | 0;
                    buff[pos++] = slotVersion[0];
                    buff[pos++] = 0x28;
                    break;
                default:
                    printf("Unknown state %d\n", state);
                    break;
            }
            break;
        default:
            printf("Unknown state %d\n", state);
            break;
    }

    msgLength = pos - COMMAND_OFFSET;
    
    if (msgLength > 0)
    {
        crc = calc_crc16(&buff[COMMAND_OFFSET], msgLength);
        write_uint16(&buff[msgLength+2], crc);
        msgLength+=4;
        
        write_uint16(&buff[0], msgLength);
        
        if (write_usb(buff, msgLength) > 0)
        {
            retVal = EXIT_SUCCESS;
        }
    }
    
    return retVal;
}


static void state_handler(void)
{
    static eState state = eStateFindDevice;
    
    switch (state)
    {
        case eStateFindDevice:
            if (open_usb() == EXIT_SUCCESS)
            {
                state = eStateInit;
            }
            else
            {
                usleep(100000);
            }
            break;
        case eStateInit:
        case eStateStop:
        case eStateSelectSlot:
        case eStateGetPatchVersion:
        case eStateGetPatchSlot:
        case eStateGetPatchNameSlot:
        case eStateGetSynthSettings:
        case eStateGetUnknown1:
        case eStateGetUnknown2:
        case eStateStart:
            if (send_command(state) == EXIT_SUCCESS)
            {
                if (int_rec() == EXIT_SUCCESS)
                {
                    state += 1;
                }
            }
            break;
        case eStatePoll:
            int_rec();
            break;
        default:
            printf("State unknown %d\n", state);
            usleep(1000);
            break;
    }
    
    if (gotBadConnectionIndication == true)
    {
        state                      = eStateFindDevice;
        gotBadConnectionIndication = false;
    }
    
    if (gotPatchChangeIndication == true)
    {
        state                      = eStateGetPatchVersion;
        gotPatchChangeIndication   = false;
    }
    
}


static void signal_handler(int sigraised)
{
    // ToDo - deal with signals properly
    printf("\nSig Handler!!! %d\n", sigraised);
   
    _exit(0);
}


static void main_thread_loop(void)
{
    reset_usb();
    
    for (;;)
    {
        state_handler();
    }
}


static int init_signals(void)
{
    int retVal = EXIT_FAILURE;
    
    if (signal(SIGINT, signal_handler) != SIG_ERR)
    {
        retVal = EXIT_SUCCESS;
    }
    
    return retVal;
}


int main(int argc, char *argv[])
{
    init_signals();
    
    main_thread_loop();

    return EXIT_SUCCESS;
}
