set mccpath=C:\Program Files (x86)\Microchip\mplabc18\v3.45\bin\
set mcex=..\..\Microchip Solutions v2011-07-14\Microchip\

set mcusb=%mcex%USB\
set flags=-p=18F4550 /i"%mcex%Include" -mL -O-

set opname=zxptester-device-firmware

"%mccpath%mcc18.exe" %flags% "main.c" -fo="main.o"
"%mccpath%mcc18.exe" %flags% "i2c_writ.c" -fo="i2c_writ.o"
"%mccpath%mcc18.exe" %flags%  "usb_descriptors.c" -fo="usb_descriptors.o"
"%mccpath%mcc18.exe" %flags%  "%mcusb%usb_device.c" -fo="usb_device.o"
"%mccpath%mcc18.exe" %flags%  "%mcusb%\HID Device Driver\usb_function_hid.c" -fo="usb_function_hid.o"
"%mccpath%mcc18.exe" %flags%  "debug.c" -fo="debug.o"
"%mccpath%mcc18.exe" %flags%  "interfacing.c" -fo="interfacing.o"
"%mccpath%mcc18.exe" %flags%  "business.c" -fo="business.o"
"%mccpath%mplink.exe" /p18F4550 "rm18f4550 - HID Bootload.lkr" "main.o" "i2c_writ.o" "usb_descriptors.o" "usb_device.o" "usb_function_hid.o" "debug.o" "interfacing.o" "business.o" /u_CRUNTIME /z__MPLAB_BUILD=1 /o"%opname%.cof" /M"%opname%.map" /W
