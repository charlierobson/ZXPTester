#include "HardwareProfile.h"
#include <delays.h>

#include "interfacing.h"

void InitInterfacing()
{
	SHIFTCLK = 0;

	// RD, WR, MEM & IORQ = 1, SHIFTCLK,DATA = 0
	LATB = 0xF0;
	TRISB = 0x03;

	TRISD = 0xff;
}

void ShiftOut(unsigned int address)
{
	int i;

	for(i = 0; i < 16; ++i)
	{
		SHIFTDAT = address & 1;
		SHIFTCLK = 1;
		address >>= 1;
		SHIFTCLK = 0;
	}
}

void MemWrite(unsigned int address, unsigned char data)
{
	ShiftOut(address);

	TRISD = 0x00;
	LATD = data;

	NMREQ = 0;
	NWR = 0;
	delayMicrosec();
	NWR = 1;
	NMREQ = 1;

	TRISD = 0xFF;
}

unsigned char MemRead(unsigned int address)
{
	unsigned char data;

	ShiftOut(address);

	TRISD = 0xFF;

	NMREQ = 0;
	NRD = 0;
	delayMicrosec();
	data = PORTD;
	NRD = 1;
	NMREQ = 1;

	return data;
}


void IOWrite(unsigned int address, unsigned char data)
{
	ShiftOut(address);

	TRISD = 0x00;
	LATD = data;

	NIORQ = 0;
	NWR = 0;
	delayMicrosec();
	NWR = 1;
	NIORQ = 1;

	TRISD = 0xFF;
}

unsigned char IORead(unsigned int address)
{
	unsigned char data;

	ShiftOut(address);

	TRISD = 0xFF;

	NIORQ = 0;
	NRD = 0;
	delayMicrosec();
	data = PORTD;
	NRD = 1;
	NIORQ = 1;

	return data;
}
