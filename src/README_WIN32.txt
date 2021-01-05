This is WIN32 port of Ideserv 0.25b (PC-Link for IDE-DOS V.91)
It was compiled and tested on WIN XP Professional SP2

THERE IS ABSOLUTELY NO WARRANTY!
Please read COPYING before use.

Ideservd is distributed with precompiled binary.
To compile it yourself you need to install Cygwin from the <http://cygwin.com>

Use make -f makefile.win32

This release is intended for FTDI USB and Ethernet interfacing, FTDI USB chip is present in IDE64 V4.1
Serial and parallel PCLink will be released soon. 

The output from the idservd.exe is by default into ideservd.log file in the application folder.
Please note, if the ideservd does not recognize file type add extra file type separated by comma e.g. file,prg

You need cygwin1.dll to run ideservd properly.

Please note, ideservd.exe does not start when IDE64 V4.1 is not running and connected to PC using the USB cable.

IDE64 USB device driver can be downloaded from the chip manufacturer
http://www.ftdichip.com/Drivers/D2XX.htm

Precompiled binary uses Cygwin 1.7.x cygwin1.dll which run on all modern 32 bit versions of Windows, except Windows CE and Windows 95/98/Me.
-------------------------------------------------------------------------------

For command line options list please run ideservd binary with '-?' or '--help'.

FILE
ideservd.exe	  FTDI USB PCLink [uses ftd2xx.dll, IDE64 V4.1]
ideservd-eth.bat  Ethernet PCLink
ideservd-pc64.bat PC64 PCLink [uses inpout32.dll]


---------------------------------------------------------------------------
Please let me know any bugreport or suggestions related to this Win32 port.
josef@ide64.org

CHANGELOG
27-09-2010
Snapshot. USB version only, the server type graphical selection is not functional yet.
4-10-2010
Added tray icon, Ethernet PCLink
19-10-2010
New pclink icon, PC64 using inpout32.dll
24-10-2010
PC64 PClink write fixed, requires Idedos V91/724 and higher.
