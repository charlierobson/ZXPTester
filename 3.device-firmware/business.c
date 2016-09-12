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
