
IDEservd, the IDEDOS 0.9x PCLink server
=======================================

This PCLink server supports:

* X1541, XE1541, XM1541 and XA1541 cables connected to IEC bus
* Parallel PC64 cable with or without FLAG line
* Null modem connection to a Duart/SwiftLink/SilverSurfer serial card
* Ethernet connection over crosslink to ETH64, RR-net or ETFE card
* USB connection to IDE64 V4.1 card (or FT245 USB FIFO)
* TCP connection to VICE USB server (IDE64 V4.1 emulation)

0.32 supports both IDEDOS 0.90 and 0.91 beta.

General options
---------------

* -b Fork into background. It'll release the terminal or hide the window.
* -C Always create comma style file types but accept dot style as well.
* -F Always create dot style file types but accept comma style as well.
* -g {group} The group to be used. Needed for dropping permissions when running
  as root.
* -u {user} The user to be used. Needed for dropping permissions when running
  as root.
* -l {file} The log file. Normally its stdout or ideservd.log.
* -n {nice} Nice level for improving reaction time.
* -P Create comma style file types if not a PRG. Only comma style file
  types are accepted. If omitted it's assumed to be PRG.
* -r {dir} Sets the root directory. On windows it's
  /cygdrive/{driveletter}/path...
* -v Verbose logging
* -? Help
* -V Version

File naming
-----------

All filenames are converted to 16 character maximum with a 3 letter file type.

Longer names are "creatively" truncated by deleting letters here and there.
Long file types are just truncated.

If duplicates come up they're filtered out, so some files might be missing.
This can only happen if the directory used was manipulated from outside and no
care was taken.

Try to avoid too long names and file types, as they're only supported for
convenience reasons.

File types are recognized after the last dot or comma. Using the '-P' option
disables dotted file types. New files are always created using a comma for file
type separation.

Simple ASCII filenames are converted to PETSCII in the range " "-"Z", "a"-"z",
"[", "]". The underscore is converted to $a4 (similar looking PETSCII
character).

PETSCII filenames are converted in the reverse way. Those not in the above
mentioned ranges are converted to Unicode equivalents (if any), or into the
0xE1xx Unicode range.

If the filesystem can't store some Unicode characters using the locale encoding
then escape sequences are used as "\uxxxx". This should normally not happen on
an UTF-8 locale (default on windows), but could happen on an ISO one for
example.

Unicode equivalents, 0xe1xx range characters and escape sequences are converted
back to PETSCII in a consistent way, so that it's not possible to tell from the
C64 side what's going on. Anything unrecognized comes up as an underscore.

Null characters are not accepted in filenames. Some reserved characters in
filenames might be converted into the 0xF0xx range by cygwin on windows.

PCLink over USB
---------------

* -m {mode} select mode, must be: usb
* -d {serial} serial number of IDE64 USB DEVICE. (optional)

On Linux the libftdi library is used, the kernel usually has the drivers.
Permissions are required for opening the device.

To give access to the IDE64 device to a regular user create an udev rule:
/etc/udev/rules.d/99-ide64.rules

Content (single line):
SUBSYSTEM=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001",
ATTRS{product}=="IDE64 USB DEVICE", OWNER="theusernametogiveaccessto"

For windows the drivers from FTDI are used. The default Microsoft drivers (if
any) will not work, it needs the D2XX drivers. You may need to update your
system if it still does not work. (http://www.ftdichip.com/Drivers/D2XX.htm)

PCLink over Ethernet
--------------------

* -m {mode} select mode, must be: eth 
* -i {ip} IP address of C64
* -N {network} Network number set in C64 setup, defaults to 0

For Ethernet you have to select an unused IP address in your network
using the "--ipaddress" option, otherwise it cannot know which
network adapter you want to use.

Setting a static arp is required, this can be done by:

Linux:  
arp -s C64IP 49:44:45:36:34:00

XP:  
arp -s C64IP 49-44-45-36-34-00

Win7:  
netsh interface ipv4 add neighbors "Local Area Connection" C64IP 49-44-45-36-34-00

The last digit of the MAC address is the network number (00 here).

Only the static arp setting needs elevated permissions. Make sure that your
firewall does not block the connection.

Routing will not work so make sure you're on the same network as the C64.
Hardwiring is preferred as there are no retransmissions for lossy links like
wireless. For direct connections a crosslink cable might be required.

PCLink over RS232-C
-------------------

Related options:

* -m {mode} select mode, one of: rs232 or rs232s
* -d {device} Serial device, e.g. COM1 or /dev/ttyS0

Normally the speed is 115.2 Kbit/s, but for SwiftLink it needs to be reduced to
38.4 Kbit/s by using the "rs232s" mode.

Make sure to have enough permissions to use the serial device. A normal
user on Linux usually needs to be in group "dialout" to get access.

PCLink over IEC bus or PC64 cable
---------------------------------

Related options:

* -m {mode} select cable type, one of: x1541, xe1541, xa1541, xm1541, pc64 or pc64s
* -p {num} printer port I/O address, defaults to 0x378 if not given
* -h Poll aggressively. Slightly better performance, but spins CPU
* -d {device} Use parport device, e.g. /dev/parport0

Direct access to parallel port of x86 style hardware is supported, in this case
remove all printer and parallel port drivers from kernel. (like lp, parport,
parport\_pc, ppdev)

It also supports ppdev, the userspace parallel port driver, by using
/dev/parport[x]. It's slower, but works on non-x86 hardware, and with
non-standard printer ports like USB adapters. This driver might not work
with the X1541 cable.

Usually a normal user needs to be in the "lp" group to be able to access
the parallel port using ppdev. Direct port access needs root of course.

On windows the inpout32.dll is used for compatibility reasons by default.
(http://www.highrez.co.uk/Downloads/InpOut32/default.htm)

If it's not available then direct access is tried. This is faster but needs
PortTalk driver installed. (http://retired.beyondlogic.org/porttalk/)

If your PC64 cable does not have the FLAG line then please connect it. If
that's not an option there's a reduced performance mode called "pc64s".

PCLink over TCP
---------------

* -m {mode} select mode, must be: vice
* -i {ip} IP address of VICE (defaults to 127.0.0.1)
* -N {port} port number of VICE (defaults to 64245)

This is used with VICE in combination with it's IDE64 USB Server and can be
used to have PCLink with an emulated V4.1 cartridge.

Make sure that your firewall does not block the connection. Otherwise it should
work.

Compiling
---------

For Linux (and similar systems), just type "make". Make sure to have libftdi-dev,
and some gcc packages are installed.

For windows cygwin needs to be installed and it's make and gcc packages. Use
"make -f Makefile.win32" for compiling.

Other systems might need modifications. For example ripping out X1541/PC64 or
USB support might be required. The rest is mostly portable.

                                                                   -soci-
