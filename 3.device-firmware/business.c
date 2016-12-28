#include "HardwareProfile.h"
#include "interfacing.h"
#include <compiler.h>

unsigned int businessContRD(void)
{
	NMREQ = 0;
	NRD = 0;
	delayMicrosec();
	NRD = 1;
	NMREQ = 1;
	return VERY_BUSY;
}

unsigned int businessContWR()
{
	NMREQ = 0;
	NWR = 0;
	delayMicrosec();
	NWR = 1;
	NMREQ = 1;
	return VERY_BUSY;
}

unsigned int businessExerciseAddr()
{
	ShiftOut(gAddress + gAddressOffset);
	NMREQ = 0;
	NMREQ = 1;

	++gAddressOffset;
	gAddressOffset &= (gLength - 1);

	return VERY_BUSY;
}

unsigned int businessExerciseData()
{
	PORTD++;
	return VERY_BUSY;
}

int toggle = 0;

unsigned int businessToggler()
{
	++toggle;
	if (toggle == 10000)
	{
		ShiftOut(gAddress);
		PORTD = gData;
	}
	else if (toggle == 20000)
	{
		ShiftOut(0);
		PORTD = 0;

		toggle = 0;
	}

	return VERY_BUSY;
}


unsigned int businessMemTest()
{
	BYTE patt = 1;
	unsigned int addr;

	for(addr = 0x4000; addr < 0xc000; ++addr, ++patt)
	{
		TRISD = 0x00;
		ShiftOut(addr);
		PORTD = patt;
		NMREQ = 0;
		NWR = 0;
		delayMicrosec();
		NWR = 1;
		NMREQ = 1;

		++patt;
	}

	for(addr = 0x4000; addr < 0xc000; ++addr, ++patt)
	{
		TRISD = 0xFF;
		ShiftOut(addr);
		NMREQ = 0;
		NRD = 0;
		delayMicrosec();
		if (PORTD != patt)
		{
			//return addr;
		}
		NRD = 1;
		NMREQ = 1;

		TRISD = 0x00;
		ShiftOut(addr);
		PORTD = ~patt;
		NMREQ = 0;
		NWR = 0;
		delayMicrosec();
		NWR = 1;
		NMREQ = 1;

		++patt;
	}

	for(addr = 0x4000; addr < 0xc000; ++addr, ++patt)
	{
		TRISD = 0xFF;
		ShiftOut(addr);
		NMREQ = 0;
		NRD = 0;
		delayMicrosec();
		if (PORTD != ~patt)
		{
			//return addr;
		}
		NRD = 1;
		NMREQ = 1;

		++patt;
	}

	return 0;	
}