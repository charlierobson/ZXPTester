#ifndef __INTERFACING_H
#define __INTERFACING_H

extern void InitInterfacing(void);
extern void ShiftOut(unsigned int address);

extern void MemWrite(unsigned int address, unsigned char data);
extern unsigned char MemRead(unsigned int address);
extern void IOWrite(unsigned int address, unsigned char data);
extern unsigned char IORead(unsigned int address);


#define SHIFTCLK LATBbits.LATB2
#define SHIFTDAT LATBbits.LATB3

#define NWR    LATBbits.LATB4
#define NRD    LATBbits.LATB5
#define NIORQ  LATBbits.LATB6
#define NMREQ  LATBbits.LATB7

// 12mhz instruction clock
#define delayMicrosec() Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
#define delayHalfMicrosec() Nop();Nop();Nop();Nop();Nop();Nop();

extern unsigned int gAddress;
extern unsigned int gLength;

extern unsigned int gAddressOffset;
#endif
