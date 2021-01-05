inpout32 should work in Win 7

For parallel port interfacing inpout32.dll <http://www.logix4u.net/inpout32.htm>
http://logix4u.net/Legacy_Ports/Parallel_Port/Inpoutx64.dll_for_WIN_XP_64_bit.html

InpOut32 a InpOutx64:
http://www.highrez.co.uk/Downloads/InpOut32/default.htm

or PortTalk V2.0 PORTTALK.SYS <http://www.beyondlogic.org/porttalk/>


################################################################################
There are currently two types of binaries which uses different access to PC LPT port.
Both should work, but ideservd-xx-io.exe need extra driver (porttalk.sys) for Win NT/2000/XP to be installed.
The installation is automatic after ideservd-xx-xp.cmd (allowio.exe) is executed for the first time
(needs admin privileges). It should need restart to work proprely. 
If you for some reason need to uninstall the driver, please use 'porttalk_uninstall.exe'.
If you don't want the driver to be installed, please use ideservd with inpout32.dll instead. 
But it has considerably worse performance.
Win 9x users, please use ideservd-xx-io.exe, no extra driver is needed.
################################################################################

inpout32 in Win Vista x64 problem solved:
http://forums.highrez.co.uk/viewtopic.php?f=7&t=130&sid=188e39ea8e34a3777d470954febd047d


ideservd-pc64.exe	  PC64 PClink [uses inpout32.dll]
NOT READY YET:
ideservd-pc64-io.exe	  PC64 PClink [direct IO access (W9x) for XP use ideservd-pc64-xp.cmd]


==========================================================
inpout32.dll
Original written by Logix4U (www.logix4u.net)
Modified for x64 compatibility and built by Phillip Gibbons (Phil@highrez.co.uk).
http://www.highrez.co.uk/Downloads/InpOut32

It does NOT support USB devices such as USB Parallel ports or even PCI parallel ports (as I am lead to believe).


The device driver is installed at runtime. To do this however needs administrator privileges.
On Vista, using UAC you can run the InstallDriver.exe in the \Win32 folder to for the driver appropriate for 
your OS to be installed. Doing so will request elevation and ask your permission (or for the administrator 
password). Once the driver is installed for the first time, it can then be used by any user *without* 
administrator privileges