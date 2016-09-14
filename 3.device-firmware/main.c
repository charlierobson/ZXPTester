/************************************************************************
	main.c

    usbGenericHidCommunication reference firmware 3_0_0_0
    Copyright (C) 2011 Simon Inns

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Email: simon.inns@gmail.com

************************************************************************/

#ifndef MAIN_C
#define MAIN_C

// Global includes
// Note: string.h is required for sprintf commands for debug
#include <string.h>
#include <usart.h>

// Local includes
#include "HardwareProfile.h"
#include "debug.h"
#include "interfacing.h"

// Microchip Application Library includes
// (expects V2.9a of the USB library from "Microchip Solutions v2011-07-14")
//
// The library location must be set in:
// Project -> Build Options Project -> Directories -> Include search path
// in order for the project to compile.
#include "./USB/usb.h"
#include "./USB/usb_function_hid.h"

// Ensure we have the correct target PIC device family
#if !defined(__18F4550) && !defined(__18F2550)
	#error "This firmware only supports either the PIC18F4550 or PIC18F2550 microcontrollers."
#endif

// Define the globals for the USB data in the USB RAM of the PIC18F*550
#pragma udata
#pragma udata USB_VARIABLES=0x500
unsigned char ReceivedDataBuffer[64];
unsigned char ToSendDataBuffer[64];
#pragma udata

USB_HANDLE USBOutHandle = 0;
USB_HANDLE USBInHandle = 0;
BOOL blinkStatusValid = FLAG_TRUE;

// PIC18F4550/PIC18F2550 configuration for the WFF Generic HID test device
#pragma config PLLDIV   = 5         // 20Mhz external oscillator
#pragma config CPUDIV   = OSC1_PLL2   
#pragma config USBDIV   = 2         // Clock source from 96MHz PLL/2
#pragma config FOSC     = HSPLL_HS
#pragma config FCMEN    = OFF
#pragma config IESO     = OFF
#pragma config PWRT     = OFF
#pragma config BOR      = ON
#pragma config BORV     = 3
#pragma config VREGEN   = ON
#pragma config WDT      = OFF
#pragma config WDTPS    = 32768
#pragma config MCLRE    = ON
#pragma config LPT1OSC  = OFF
#pragma config PBADEN   = OFF
// #pragma config CCP2MX   = ON
#pragma config STVREN   = ON
#pragma config LVP      = OFF
// #pragma config ICPRT    = OFF
#pragma config XINST    = OFF
#pragma config CP0      = OFF
#pragma config CP1      = OFF
// #pragma config CP2      = OFF
// #pragma config CP3      = OFF
#pragma config CPB      = OFF
// #pragma config CPD      = OFF
#pragma config WRT0     = OFF
#pragma config WRT1     = OFF
// #pragma config WRT2     = OFF
// #pragma config WRT3     = OFF
#pragma config WRTB     = OFF
#pragma config WRTC     = OFF
// #pragma config WRTD     = OFF
#pragma config EBTR0    = OFF
#pragma config EBTR1    = OFF
// #pragma config EBTR2    = OFF
// #pragma config EBTR3    = OFF
#pragma config EBTRB    = OFF

// Private function prototypes
static void initialisePic(void);
void processUsbCommands(void);
void applicationInit(void);
void USBCBSendResume(void);
void highPriorityISRCode();
void lowPriorityISRCode();

// Remap vectors for compatibilty with Microchip USB boot loaders
#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x1000
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x1008
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x1018
#elif defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)	
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x800
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x808
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x818
#else	
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x00
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x08
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x18
#endif

#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER) || defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
	extern void _startup (void);
	#pragma code REMAPPED_RESET_VECTOR = REMAPPED_RESET_VECTOR_ADDRESS
	void _reset (void)
	{
	    _asm goto _startup _endasm
	}
#endif

#pragma code REMAPPED_HIGH_INTERRUPT_VECTOR = REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS
void Remapped_High_ISR (void)
{
     _asm goto highPriorityISRCode _endasm
}

#pragma code REMAPPED_LOW_INTERRUPT_VECTOR = REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS
void Remapped_Low_ISR (void)
{
     _asm goto lowPriorityISRCode _endasm
}

#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER) || defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
#pragma code HIGH_INTERRUPT_VECTOR = 0x08
void High_ISR (void)
{
     _asm goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS _endasm
}

#pragma code LOW_INTERRUPT_VECTOR = 0x18
void Low_ISR (void)
{
     _asm goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS _endasm
}
#endif

#pragma code

// High-priority ISR handling function
#pragma interrupt highPriorityISRCode
void highPriorityISRCode()
{
	// Application specific high-priority ISR code goes here
	
	#if defined(USB_INTERRUPT)
		// Perform USB device tasks
		USBDeviceTasks();
	#endif

}

// Low-priority ISR handling function
#pragma interruptlow lowPriorityISRCode
void lowPriorityISRCode()
{
	// Application specific low-priority ISR code goes here
}

// Variables required for keeping track of a bulk receive command
UINT8 bulkReceiveFlag = FLAG_FALSE;
INT bulkReceivePacketCounter = 0;
INT bulkReceiveExpectedPackets = 0;

// Variables required for keeping track of a bulk send command
UINT8 bulkSendFlag = FLAG_FALSE;
INT bulkSendPacketCounter = 0;
INT bulkSendExpectedPackets = 0;

// String for creating debug messages
char debugString[64];

// global pointer for multibyte operations
unsigned char gData;
unsigned int gAddress;
unsigned int gLength;
unsigned int gAddressOffset;

// function pointer to bulk reception processor
void (*bulkFunction)(void) = NULL;

unsigned int blinkCounter;

unsigned int Unbusy()
{
	return NOT_BUSY;
}

unsigned int (*busyFn)(void) = Unbusy;

volatile unsigned int trigRate = 127;

// Main program entry point
void main(void)
{   
	// Initialise and configure the PIC ready to go
    initialisePic();

	// If we are running in interrupt mode attempt to attach the USB device
    #if defined(USB_INTERRUPT)
        USBDeviceAttach();
    #endif
	
	// Initialise the debug log functions
    debugInitialise();
    
	sprintf(debugString, "ZXpandTester by SirMorris");
	debugOut(debugString);

	sprintf(debugString, "Based on work (C)2011 Simon Inns");
	debugOut(debugString);
	
	sprintf(debugString, "USB Device Initialised");
	debugOut(debugString);
	
    // Show that we are up and running
    mStatusLED0_on();
	
	// Main processing loop
    while(1)
    {
		++blinkCounter;
		mScopeTrigger = (blinkCounter & trigRate) == 0;
		mStatusLED0 = (blinkCounter & busyFn()) != 0;

        #if defined(USB_POLLING)
			// If we are in polling mode the USB device tasks must be processed here
			// (otherwise the interrupt is performing this task)
	        USBDeviceTasks();
        #endif
    	
    	// Process USB Commands
        processUsbCommands();  
        
        // Note: Other application specific actions can be placed here      
    }
}

// Initialise the PIC
static void initialisePic(void)
{
    // PIC port set up --------------------------------------------------------

	// Default all pins to digital
    ADCON1 = 0x0F;
	CMCON |= 7;

	// Configure ports as inputs as we don't want to hurt the genie bus	
	TRISA = 0b11111111;
	TRISB = 0b11111111;
	TRISC = 0b11111111;
	TRISD = 0b11111111;
	TRISE = 0b11111111;

	// Clear all ports
	PORTA = 0b00000000;
	PORTB = 0b00000000;
	PORTC = 0b00000000;
	PORTD = 0b00000000;
	PORTE = 0b00000000;

    // Application specific initialisation
    applicationInit();

    // Initialise the USB device
    USBDeviceInit();
}

// Application specific device initialisation
void applicationInit(void)
{
	// Initialise the status LEDs
	mInitStatusLeds();
	mInitScopeTaps();

	mStatusLED0_on();

    // Initialize the variable holding the USB handle for the last transmission
    USBOutHandle = 0;
    USBInHandle = 0;

	InitInterfacing();

	// 115200 with 20mhz crystal w/PLL : FOSC = 48mhz
	OpenUSART(	(USART_TX_INT_OFF | USART_RX_INT_OFF |
				 USART_ASYNCH_MODE | USART_EIGHT_BIT | USART_CONT_RX |
				 USART_BRGH_HIGH), 25);
}


extern unsigned int businessContRD();
extern unsigned int businessContWR();
extern unsigned int businessExerciseAddr();
extern unsigned int businessExerciseData();
extern unsigned int businessToggler();


// bulk handlers
//
void MemWriteMultiple()
{
	int i;
	int count = (gLength > 63) ? 64 : gLength;

	gLength -= count;

	for (i = 0; i < count; ++i, ++gAddress)
	{
		MemWrite(gAddress, ReceivedDataBuffer[i]);
	}
}

void MemReadMultiple()
{
	int i;
	int count = (gLength > 63) ? 64 : gLength;

	gLength -= count;

	for (i = 0; i < count; ++i, ++gAddress)
	{
		ToSendDataBuffer[i] = MemRead(gAddress);
	}
}

// Process USB commands
void processUsbCommands(void)
{   
	// Note: For all tests we expect to receive a 64 byte packet containing
	// the command in byte[0] and then the numbers 0-62 in bytes 1-63.
	UINT8 bufferPointer;
    UINT8 expectedData;
    UINT8 dataReceivedOk;
    UINT8 dataSentOk;
    
    // Check if we are in the configured state; otherwise just return
    if((USBDeviceState < CONFIGURED_STATE) || (USBSuspendControl == 1))
    {
	    // We are not configured, set LED1 to off to show status
	    mStatusLED1_off();
	    return;
	}
	
	// We are configured, show state in LED1
	mStatusLED1_on();

	// Check if data was received from the host.
    if(!HIDRxHandleBusy(USBOutHandle))
    {   
	    // Test to see if we are in bulk send/receieve mode, or if we are waiting for a command
	    if (bulkSendFlag == FLAG_TRUE || bulkReceiveFlag == FLAG_TRUE)
	    {
		    // We are either bulk sending or receieving
		    
		    // If we are bulk sending, check that we are not busy and send the next packet
		    if (bulkSendFlag == FLAG_TRUE && !HIDTxHandleBusy(USBInHandle))
		    {
				// bulk function will fill the buffer
				if (bulkFunction != NULL) bulkFunction();

		        // Transmit the response to the host
                USBInHandle = HIDTxPacket(HID_EP,(BYTE*)&ToSendDataBuffer[0],64);

				++bulkSendPacketCounter;
				if (bulkSendPacketCounter == bulkSendExpectedPackets)
				{
					// All done, indicate success and go back to command mode
					bulkSendFlag = FLAG_FALSE;
					bulkFunction = NULL;

		            sprintf(debugString, "Bulk op complete");
					debugOut(debugString);
				}	
			}
			
			if (bulkReceiveFlag == FLAG_TRUE)
			{
				// The received data buffer is already filled by the USB stack
				if (bulkFunction != NULL) bulkFunction();

				bulkReceivePacketCounter++;
				if (bulkReceivePacketCounter == bulkReceiveExpectedPackets)
				{
					bulkReceiveFlag = FLAG_FALSE;
					bulkFunction = NULL;

		            sprintf(debugString, "Bulk op complete");
					debugOut(debugString);
				}
			}
		}
		else
		{
			// Command mode    
	        switch(ReceivedDataBuffer[0])
			{
				case 0x10:	// Debug information request from host
					// Copy any waiting debug text to the send data buffer
					copyDebugToSendBuffer((BYTE*)&ToSendDataBuffer[0]);
				        
			        // Transmit the response to the host
	                if(!HIDTxHandleBusy(USBInHandle))
					{
						USBInHandle = HIDTxPacket(HID_EP,(BYTE*)&ToSendDataBuffer[0],64);
					}
					break;
					
				// Place application specific commands here:
				
	            case 0x80:  // WRITE BYTE
				{
					int address = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
					unsigned char data = ReceivedDataBuffer[3];

					MemWrite(address, data);

	            	sprintf(debugString, "Byte $%02X written to $%04X", data, address);
					debugOut(debugString);
				}
            	break;

	            case 0x81:  // READ BYTE
				{
					int address = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];

					unsigned char data = MemRead(address);

		            ToSendDataBuffer[0] = data;

	                if(!HIDTxHandleBusy(USBInHandle))
					{
						USBInHandle = HIDTxPacket(HID_EP,(BYTE*)&ToSendDataBuffer[0],64);
					}

	            	sprintf(debugString, "Read $%02X from $%04X", data, address);
					debugOut(debugString);
				}
            	break;

	            case 0x82:
				{
					gAddress = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
					gLength = ((int)ReceivedDataBuffer[3] << 8) + ReceivedDataBuffer[4];

		            sprintf(debugString, "Block write $%04X..$%04x incl.", gAddress, gAddress + gLength - 1);
					debugOut(debugString);

					bulkFunction = MemWriteMultiple;

		            bulkReceiveExpectedPackets = (gLength + 63) / 64;
		            bulkReceivePacketCounter = 0;
		            bulkReceiveFlag = FLAG_TRUE;
		        }
            	break;

	            case 0x83:
				{
					gAddress = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
					gLength = ((int)ReceivedDataBuffer[3] << 8) + ReceivedDataBuffer[4];

		            sprintf(debugString, "Block read $%04X..$%04x incl.", gAddress, gAddress + gLength - 1);
					debugOut(debugString);

					bulkFunction = MemReadMultiple;

		            bulkSendExpectedPackets = (gLength + 63) / 64;
		            bulkSendPacketCounter = 0;
		            bulkSendFlag = FLAG_TRUE;
				}
            	break;

	            case 0x84:  // IO WRITE
				{
					int address = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
					unsigned char data = ReceivedDataBuffer[3];

					IOWrite(address, data);

	            	sprintf(debugString, "IO write $%02X to port $%04X", data, address);
					debugOut(debugString);
				}
            	break;

	            case 0x85:  // IO READ
				{
					int address = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];

					unsigned char data = IORead(address);

		            ToSendDataBuffer[0] = data;

	                if(!HIDTxHandleBusy(USBInHandle))
					{
						USBInHandle = HIDTxPacket(HID_EP,(BYTE*)&ToSendDataBuffer[0],64);
					}

	            	sprintf(debugString, "IO Read $%02X from port $%04X", data, address);
					debugOut(debugString);
				}
            	break;

				case 0xE0:
					InitInterfacing();
					gAddress = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
					ShiftOut(gAddress);
					TRISD = 0xFF; // should be input as genie will be driving the bus - ding ding!
		            sprintf(debugString, "E0 Repeat read from $%04X", gAddress);
					debugOut(debugString);
					busyFn = businessContRD;
					break;

				case 0xE1:
					InitInterfacing();
					gAddress = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
					ShiftOut(gAddress);
					LATD = ReceivedDataBuffer[3];
					TRISD = 0x00;
		            sprintf(debugString, "E1 Repeat write $%04X <= $%02X", gAddress, ReceivedDataBuffer[3]);
					debugOut(debugString);
					busyFn = businessContWR;
					break;

				case 0xE2:
					InitInterfacing();
					gAddress = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
					gLength = ((int)ReceivedDataBuffer[3] << 8) + ReceivedDataBuffer[4];
		            sprintf(debugString, "E2 Addr exercise $%04X..$%04X incl.", gAddress, gAddress + gLength - 1);
					debugOut(debugString);
					busyFn = businessExerciseAddr;
					break;

				case 0xE3:
					InitInterfacing();
					TRISD = 0x00;
		            sprintf(debugString, "E3 Data exercise");
					debugOut(debugString);
					busyFn = businessExerciseData;
					break;

				case 0xE4:
					InitInterfacing();
					TRISD = 0x00;
					gAddress = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
					gData = ReceivedDataBuffer[4];

		            sprintf(debugString, "E4 Rept Toggle %04x %02x", gAddress, gData);
					debugOut(debugString);
					busyFn = businessToggler;
					break;

				;

				case 0xF0:
					InitInterfacing();
		            sprintf(debugString, "Unbusy");
					debugOut(debugString);
					busyFn = Unbusy;
					break;

				;

				case 0xFC:
					trigRate = ((int)ReceivedDataBuffer[1] << 8) + ReceivedDataBuffer[2];
		            sprintf(debugString, "Config\nTR %04X", trigRate);
					debugOut(debugString);
					break;

	            default:	// Unknown command received
		            sprintf(debugString, "Unknown command block type: %02X", ReceivedDataBuffer[0]);
					debugOut(debugString);
	           		break;
			}
		}
		
		// Only re-arm the OUT endpoint if we are not bulk sending
		if (bulkSendFlag == FLAG_FALSE)
		{      
	        // Re-arm the OUT endpoint for the next packet
	        USBOutHandle = HIDRxPacket(HID_EP,(BYTE*)&ReceivedDataBuffer,64);
  		}
  	}
}


// USB Callback handling routines -----------------------------------------------------------

// Call back that is invoked when a USB suspend is detected
void USBCBSuspend(void)
{
}

// This call back is invoked when a wakeup from USB suspend is detected.
void USBCBWakeFromSuspend(void)
{
}

// The USB host sends out a SOF packet to full-speed devices every 1 ms.
void USBCB_SOF_Handler(void)
{
    // No need to clear UIRbits.SOFIF to 0 here. Callback caller is already doing that.
}

// The purpose of this callback is mainly for debugging during development.
// Check UEIR to see which error causes the interrupt.
void USBCBErrorHandler(void)
{
    // No need to clear UEIR to 0 here.
    // Callback caller is already doing that.
}

// Check other requests callback
void USBCBCheckOtherReq(void)
{
    USBCheckHIDRequest();
}

// Callback function is called when a SETUP, bRequest: SET_DESCRIPTOR request arrives.
void USBCBStdSetDscHandler(void)
{
    // You must claim session ownership if supporting this request
}

//This function is called when the device becomes initialized
void USBCBInitEP(void)
{
    // Enable the HID endpoint
    USBEnableEndpoint(HID_EP,USB_IN_ENABLED|USB_OUT_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
    
    // Re-arm the OUT endpoint for the next packet
    USBOutHandle = HIDRxPacket(HID_EP,(BYTE*)&ReceivedDataBuffer,64);
}

// Send resume call-back
void USBCBSendResume(void)
{
    static WORD delay_count;
    
    // Verify that the host has armed us to perform remote wakeup.
    if(USBGetRemoteWakeupStatus() == FLAG_TRUE) 
    {
        // Verify that the USB bus is suspended (before we send remote wakeup signalling).
        if(USBIsBusSuspended() == FLAG_TRUE)
        {
            USBMaskInterrupts();
            
            // Bring the clock speed up to normal running state
            USBCBWakeFromSuspend();
            USBSuspendControl = 0; 
            USBBusIsSuspended = FLAG_FALSE;

            // Section 7.1.7.7 of the USB 2.0 specifications indicates a USB
            // device must continuously see 5ms+ of idle on the bus, before it sends
            // remote wakeup signalling.  One way to be certain that this parameter
            // gets met, is to add a 2ms+ blocking delay here (2ms plus at 
            // least 3ms from bus idle to USBIsBusSuspended() == FLAG_TRUE, yeilds
            // 5ms+ total delay since start of idle).
            delay_count = 3600U;        
            do
            {
                delay_count--;
            } while(delay_count);
            
            // Start RESUME signaling for 1-13 ms
            USBResumeControl = 1;
            delay_count = 1800U;
            do
            {
                delay_count--;
            } while(delay_count);
            USBResumeControl = 0;

            USBUnmaskInterrupts();
        }
    }
}

// USB callback function handler
BOOL USER_USB_CALLBACK_EVENT_HANDLER(USB_EVENT event, void *pdata, WORD size)
{
    switch(event)
    {
        case EVENT_TRANSFER:
            // Application callback tasks and functions go here
            break;
        case EVENT_SOF:
            USBCB_SOF_Handler();
            break;
        case EVENT_SUSPEND:
            USBCBSuspend();
            break;
        case EVENT_RESUME:
            USBCBWakeFromSuspend();
            break;
        case EVENT_CONFIGURED: 
            USBCBInitEP();
            break;
        case EVENT_SET_DESCRIPTOR:
            USBCBStdSetDscHandler();
            break;
        case EVENT_EP0_REQUEST:
            USBCBCheckOtherReq();
            break;
        case EVENT_BUS_ERROR:
            USBCBErrorHandler();
            break;
        default:
            break;
    }      
    return FLAG_TRUE; 
}

#endif
